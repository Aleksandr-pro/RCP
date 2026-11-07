#include <android/log.h>
#include <android/input.h>
#include <android/native_window.h>
#include <android_native_app_glue.h>
#include <jni.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>

#include "renderer/VulkanRenderer.h"
#include "physics/VerticalPhysics.h"
#include "physics/SurfaceBounce.h"

namespace
{
    constexpr const char* LOG_TAG = "ATLAS";

    struct EngineState
    {
        VulkanRenderer renderer;
        atlas::physics::Platform platform{0.0f, -1.0f, 0.0f, 2.0f, 0.1f, 1.0f};
        std::vector<atlas::physics::Box> boxes{
            {-0.6f, 0.0f, 0.0f, 0.3f, 0.3f, 0.3f, 0.0f},
            {-0.6f, 1.0f, 0.0f, 0.3f, 0.3f, 0.3f, 0.0f}
        };
        const std::vector<atlas::physics::Box> initialBoxes = boxes;
        atlas::physics::Box bouncingBox{0.7f, 0.4f, 0.0f, 0.3f, 0.3f, 0.3f, 0.0f};
        const atlas::physics::Box initialBouncingBox = bouncingBox;
        atlas::physics::ContactMaterial contact{};
        size_t bouncingObjectIndex = 0;
        std::vector<size_t> boxObjectIndices;
        bool physicsConfigured = false;
        bool physicsRunning = false;
        double physicsAccumulator = 0.0;
        std::chrono::steady_clock::time_point physicsTime{};

        bool isActive = false;
        bool hasFocus = false;

        float angle = 0.55f;
        float tiltAngle = -0.35f;
        float cameraDistance = 3.0f;

        bool touchActive = false;
        int32_t activePointerId = -1;
        float lastTouchX = 0.0f;
        float lastTouchY = 0.0f;

        bool pinchActive = false;
        int32_t pinchPointerId1 = -1;
        int32_t pinchPointerId2 = -1;
        float lastPinchDistance = 0.0f;

        std::chrono::steady_clock::time_point lastFrameTime{};
        std::chrono::steady_clock::time_point nextFrameTime{};
        std::chrono::steady_clock::time_point statsStartTime{};
        uint32_t presentedFrames = 0;

        std::chrono::steady_clock::time_point lastPresentedTime{};

        uint32_t measuredIntervals = 0;
        uint32_t intervalsOver50Ms = 0;
        double maxIntervalMs = 0.0;

        double totalWorkMs = 0.0;
        double maxWorkMs = 0.0;
    };

    void logInfo(const char* message)
    {
        __android_log_print(
            ANDROID_LOG_INFO,
            LOG_TAG,
            "%s",
            message
        );
    }

    void logError(const char* message)
    {
        __android_log_print(
            ANDROID_LOG_ERROR,
            LOG_TAG,
            "%s",
            message
        );
    }

    bool validBouncingBox(const EngineState& state)
    {
        const auto& box = state.bouncingBox;
        const auto& p = state.platform;
        // validate checks finite dimensions, initial height and X/Z overlap.
        if (!atlas::physics::validate(p, std::vector<atlas::physics::Box>{box})) return false;
        for (const auto& other : state.boxes)
        {
            if (std::abs(box.x - other.x) < box.halfX + other.halfX &&
                std::abs(box.z - other.z) < box.halfZ + other.halfZ)
                return false;
        }
        return true;
    }

    bool bindPhysicsScene(EngineState* state)
    {
        const auto& objects = state->renderer.sceneObjects();
        if (objects.size() != state->initialBoxes.size() + 2) return false;
        std::vector<bool> used(objects.size(), false);
        auto findObject = [&](float x, float y, float z, float sx, float sy, float sz) -> size_t
        {
            for (size_t i = 0; i < objects.size(); ++i)
            {
                const auto& o = objects[i];
                auto near = [](float a, float b) { return std::abs(a-b) < 0.00001f; };
                if (!used[i] && o.meshName == "cube" && near(o.rotationOffset, 0.0f) &&
                    near(o.x,x) && near(o.y,y) && near(o.z,z) &&
                    near(o.scale,sx) && near(o.scaleY,sy) && near(o.scaleZ,sz))
                { used[i] = true; return i; }
            }
            return objects.size();
        };
        const auto& p = state->platform;
        if (findObject(p.x,p.y,p.z,2*p.halfX,2*p.halfY,2*p.halfZ) == objects.size()) return false;
        state->boxObjectIndices.clear();
        for (const auto& box : state->initialBoxes)
        {
            const size_t i = findObject(box.x,box.y,box.z,2*box.halfX,2*box.halfY,2*box.halfZ);
            if (i == objects.size()) return false;
            state->boxObjectIndices.push_back(i);
        }
        const auto& bounce = state->initialBouncingBox;
        state->bouncingObjectIndex = findObject(bounce.x, bounce.y, bounce.z,
                                                2*bounce.halfX, 2*bounce.halfY, 2*bounce.halfZ);
        return state->bouncingObjectIndex != objects.size();
    }

    bool syncPhysicsScene(EngineState* state)
    {
        if (state->boxes.size() != state->boxObjectIndices.size()) return false;
        for (size_t i = 0; i < state->boxes.size(); ++i)
            if (!state->renderer.setObjectY(state->boxObjectIndices[i], state->boxes[i].y)) return false;
        return state->renderer.setObjectY(state->bouncingObjectIndex, state->bouncingBox.y);
    }

    // Для версии 0.3 при изменении окна пересоздаём весь рендерер.
    // Это не самое быстрое решение, но оно проще для первой проверки.
    bool restartRenderer(android_app* app, EngineState* state)
    {
        if (app == nullptr ||
            state == nullptr ||
            app->window == nullptr)
        {
            logError("ATLAS: невозможно запустить рендерер без окна.");
            return false;
        }

        // Если окно изменилось во время создания swapchain,
        // разрешаем одну повторную попытку.
        constexpr int maxAttempts = 2;

        for (int attempt = 0; attempt < maxAttempts; ++attempt)
        {
            __android_log_print(
                ANDROID_LOG_INFO,
                LOG_TAG,
                "ATLAS: создание Vulkan-рендерера, попытка %d/%d.",
                attempt + 1,
                maxAttempts
            );

            if (!state->renderer.initialize(
                    app->window,
                    app->activity->assetManager
                ))
            {
                logError("ATLAS: ошибка запуска Vulkan-рендерера.");
                return false;
            }

            if ((!state->physicsConfigured && (!atlas::physics::validate(state->platform, state->boxes) || !validBouncingBox(*state))) ||
                !bindPhysicsScene(state) || !syncPhysicsScene(state) ||
                !state->renderer.updateRotation(state->angle, state->tiltAngle, state->cameraDistance))
            {
                logError("ATLAS: недопустимое физическое состояние или связь объектов сцены.");
                state->renderer.shutdown();
                return false;
            }
            if (!state->physicsConfigured)
            {
                state->physicsConfigured = true;
                state->physicsRunning = true;
            }
            state->physicsAccumulator = 0.0;
            state->physicsTime = std::chrono::steady_clock::now();

            if (state->renderer.drawFrame())
            {
                return true;
            }

            // Считываем признак до shutdown(), который его сбрасывает.
            const bool outOfDate =
                state->renderer.isSwapchainOutOfDate();

            state->renderer.shutdown();

            if (!outOfDate)
            {
                logError("ATLAS: первый Vulkan-кадр не выведен.");
                return false;
            }

            logInfo("ATLAS: swapchain устарел, повторяем создание.");
        }

        __android_log_print(
            ANDROID_LOG_WARN,
            LOG_TAG,
            "ATLAS: окно ещё меняется; ожидаем следующее событие окна."
        );

        return false;
    }

    void redraw(android_app* app, EngineState* state)
    {
        if (app == nullptr ||
            state == nullptr ||
            app->window == nullptr)
        {
            return;
        }

        if (!state->renderer.isInitialized())
        {
            restartRenderer(app, state);
            return;
        }

        if (state->renderer.drawFrame())
        {
            return;
        }

        if (state->renderer.isSwapchainOutOfDate())
        {
            logInfo("ATLAS: пересоздание устаревшего swapchain.");
            restartRenderer(app, state);
        }
        else
        {
            logError("ATLAS: ошибка отрисовки Vulkan.");
            state->renderer.shutdown();
        }
    }

    void updateFpsOverlay(android_app* app, float fps, float intervalMs, uint32_t frames)
    {
        if (app == nullptr || app->activity == nullptr)
        {
            return;
        }

        JavaVM* vm = app->activity->vm;
        JNIEnv* env = nullptr;
        const jint status = vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
        const bool attached = status == JNI_EDETACHED;
        if (attached && vm->AttachCurrentThread(&env, nullptr) != JNI_OK)
        {
            logError("ATLAS: не удалось подключиться к Java для FPS.");
            return;
        }
        if (!attached && status != JNI_OK)
        {
            logError("ATLAS: не удалось получить Java-окружение для FPS.");
            return;
        }

        jobject activity = app->activity->clazz;
        jclass activityClass = env->GetObjectClass(activity);
        jmethodID updateMethod = activityClass == nullptr ? nullptr :
            env->GetMethodID(activityClass, "updateFps", "(FFI)V");
        if (updateMethod != nullptr)
        {
            env->CallVoidMethod(activity, updateMethod, fps, intervalMs, static_cast<jint>(frames));
        }
        if (env->ExceptionCheck())
        {
            env->ExceptionDescribe();
            env->ExceptionClear();
            logError("ATLAS: не удалось обновить FPS на экране.");
        }
        if (activityClass != nullptr)
        {
            env->DeleteLocalRef(activityClass);
        }
        if (attached)
        {
            vm->DetachCurrentThread();
        }
    }

    void recordPresentedFrame(
        android_app* app,
        EngineState* state,
        double workMs
    )
    {
        if (state == nullptr)
        {
            return;
        }

        const auto now = std::chrono::steady_clock::now();

        if (state->statsStartTime ==
            std::chrono::steady_clock::time_point{})
        {
            state->statsStartTime = now;
        }

        // Первый кадр после сброса не имеет предыдущего интервала.
        if (state->lastPresentedTime !=
            std::chrono::steady_clock::time_point{})
        {
            const double intervalMs =
                std::chrono::duration<double, std::milli>(
                    now - state->lastPresentedTime
                ).count();

            ++state->measuredIntervals;

            state->maxIntervalMs =
                std::max(state->maxIntervalMs, intervalMs);

            if (intervalMs > 50.0)
            {
                ++state->intervalsOver50Ms;
            }
        }

        state->lastPresentedTime = now;

        ++state->presentedFrames;
        state->totalWorkMs += workMs;
        state->maxWorkMs = std::max(state->maxWorkMs, workMs);

        const double elapsedSeconds =
            std::chrono::duration<double>(
                now - state->statsStartTime
            ).count();

        if (elapsedSeconds >= 5.0 && state->presentedFrames > 0)
        {
            const double fps =
                static_cast<double>(state->presentedFrames) /
                elapsedSeconds;

            const double averageIntervalMs =
                1000.0 * elapsedSeconds /
                static_cast<double>(state->presentedFrames);

            const double averageWorkMs =
                state->totalWorkMs /
                static_cast<double>(state->presentedFrames);

            __android_log_print(
                ANDROID_LOG_INFO,
                LOG_TAG,
                "ATLAS stats: FPS=%.1f, средний интервал=%.1f мс, "
                "макс. интервал=%.1f мс, свыше 50 мс=%u/%u, "
                "работа кадра: средняя=%.2f мс, макс.=%.2f мс",
                fps,
                averageIntervalMs,
                state->maxIntervalMs,
                state->intervalsOver50Ms,
                state->measuredIntervals,
                averageWorkMs,
                state->maxWorkMs
            );

            // Существующую панель не меняем.
            updateFpsOverlay(
                app,
                static_cast<float>(fps),
                static_cast<float>(averageIntervalMs),
                state->presentedFrames
            );

            state->statsStartTime = now;
            state->presentedFrames = 0;
            state->measuredIntervals = 0;
            state->intervalsOver50Ms = 0;
            state->maxIntervalMs = 0.0;
            state->totalWorkMs = 0.0;
            state->maxWorkMs = 0.0;

            // lastPresentedTime сохраняем, чтобы не потерять
            // интервал на границе двух периодов статистики.
        }
    }

    void updateAndDraw(android_app* app, EngineState* state)
    {
        if (app == nullptr ||
            state == nullptr ||
            app->window == nullptr ||
            !state->renderer.isInitialized())
        {
            return;
        }

        const auto now = std::chrono::steady_clock::now();
        const auto workStartTime = now;

        if (state->physicsRunning && state->physicsTime != std::chrono::steady_clock::time_point{})
        {
            constexpr double fixedStep = 1.0 / 60.0;
            const double elapsed = std::chrono::duration<double>(now - state->physicsTime).count();
            state->physicsAccumulator = std::min(state->physicsAccumulator + std::max(elapsed, 0.0), 0.25);
            while (state->physicsAccumulator + 1e-12 >= fixedStep)
            {
                if (!atlas::physics::step(state->platform, state->boxes, 1.0f / 60.0f))
                {
                    logError("ATLAS: ошибка шага физики; симуляция остановлена.");
                    state->physicsRunning = false;
                    state->physicsAccumulator = 0.0;
                    break;
                }
                if (!atlas::physics::stepBouncingBox(state->platform, state->bouncingBox,
                                                      state->contact, 1.0f / 60.0f))
                {
                    logError("ATLAS: ошибка отскока; симуляция остановлена.");
                    state->physicsRunning = false;
                    state->physicsAccumulator = 0.0;
                    break;
                }
                state->physicsAccumulator -= fixedStep;
            }
        }
        state->physicsTime = now;
        if (!syncPhysicsScene(state))
        {
            logError("ATLAS: не удалось обновить положения кубов.");
            state->renderer.shutdown();
            return;
        }

        if (!state->renderer.updateRotation(
                state->angle,
                state->tiltAngle,
                state->cameraDistance
            ))
        {
            logError("ATLAS: не удалось обновить команды кадра.");
            state->renderer.shutdown();
            return;
        }

        if (state->renderer.drawFrame())
        {
            const auto workEndTime = std::chrono::steady_clock::now();

            const double workMs =
                std::chrono::duration<double, std::milli>(
                    workEndTime - workStartTime
                ).count();

            recordPresentedFrame(app, state, workMs);
            return;
        }

        if (state->renderer.isSwapchainOutOfDate())
        {
            logInfo("ATLAS: swapchain устарел во время анимации.");
            restartRenderer(app, state);
        }
        else
        {
            logError("ATLAS: ошибка отрисовки во время анимации.");
            state->renderer.shutdown();
        }
    }

    int32_t handleInput(android_app* app, AInputEvent* event)
    {
        if (app == nullptr || event == nullptr)
        {
            return 0;
        }

        auto* state = static_cast<EngineState*>(app->userData);

        if (state == nullptr ||
            AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION)
        {
            return 0;
        }

        const int32_t action = AMotionEvent_getAction(event);
        const int32_t actionMasked =
            action & AMOTION_EVENT_ACTION_MASK;

        const size_t pointerCount =
            AMotionEvent_getPointerCount(event);

        switch (actionMasked)
        {
            case AMOTION_EVENT_ACTION_DOWN:
            {
                state->pinchActive = false;
                state->touchActive = true;

                state->activePointerId =
                    AMotionEvent_getPointerId(event, 0);

                state->lastTouchX = AMotionEvent_getX(event, 0);
                state->lastTouchY = AMotionEvent_getY(event, 0);

                return 1;
            }

            case AMOTION_EVENT_ACTION_POINTER_DOWN:
            {
                if (pointerCount < 2)
                {
                    return 1;
                }

                // Для первой версии используем первые два пальца.
                const float x1 = AMotionEvent_getX(event, 0);
                const float y1 = AMotionEvent_getY(event, 0);
                const float x2 = AMotionEvent_getX(event, 1);
                const float y2 = AMotionEvent_getY(event, 1);

                const float dx = x2 - x1;
                const float dy = y2 - y1;

                state->lastPinchDistance = std::sqrt(dx * dx + dy * dy);

                state->pinchPointerId1 =
                    AMotionEvent_getPointerId(event, 0);
                state->pinchPointerId2 =
                    AMotionEvent_getPointerId(event, 1);

                state->pinchActive = state->lastPinchDistance > 1.0f;

                // При масштабировании не вращаем сцену.
                state->touchActive = false;
                state->activePointerId = -1;

                return 1;
            }

            case AMOTION_EVENT_ACTION_MOVE:
            {
                if (state->pinchActive)
                {
                    int32_t index1 = -1;
                    int32_t index2 = -1;

                    for (size_t i = 0; i < pointerCount; ++i)
                    {
                        const int32_t id =
                            AMotionEvent_getPointerId(event, i);

                        if (id == state->pinchPointerId1)
                        {
                            index1 = static_cast<int32_t>(i);
                        }

                        if (id == state->pinchPointerId2)
                        {
                            index2 = static_cast<int32_t>(i);
                        }
                    }

                    if (index1 < 0 || index2 < 0)
                    {
                        state->pinchActive = false;
                        return 1;
                    }

                    const float x1 =
                        AMotionEvent_getX(event, static_cast<size_t>(index1));
                    const float y1 =
                        AMotionEvent_getY(event, static_cast<size_t>(index1));
                    const float x2 =
                        AMotionEvent_getX(event, static_cast<size_t>(index2));
                    const float y2 =
                        AMotionEvent_getY(event, static_cast<size_t>(index2));

                    const float dx = x2 - x1;
                    const float dy = y2 - y1;
                    const float distance = std::sqrt(dx * dx + dy * dy);

                    if (distance > 1.0f &&
                        state->lastPinchDistance > 1.0f)
                    {
                        // Пальцы расходятся → расстояние уменьшается.
                        state->cameraDistance *=
                            state->lastPinchDistance / distance;

                        state->cameraDistance = std::clamp(
                            state->cameraDistance,
                            2.5f,
                            10.0f
                        );

                        state->lastPinchDistance = distance;
                    }

                    return 1;
                }

                if (!state->touchActive || app->window == nullptr)
                {
                    return 0;
                }

                for (size_t i = 0; i < pointerCount; ++i)
                {
                    if (AMotionEvent_getPointerId(event, i) !=
                        state->activePointerId)
                    {
                        continue;
                    }

                    const float currentX = AMotionEvent_getX(event, i);
                    const float currentY = AMotionEvent_getY(event, i);

                    const float deltaX = currentX - state->lastTouchX;
                    const float deltaY = currentY - state->lastTouchY;

                    state->lastTouchX = currentX;
                    state->lastTouchY = currentY;

                    const int32_t width =
                        ANativeWindow_getWidth(app->window);

                    const int32_t height =
                        ANativeWindow_getHeight(app->window);

                    if (width > 0)
                    {
                        constexpr float fullTurn = 6.2831853f;

                        state->angle +=
                            deltaX / static_cast<float>(width) * fullTurn;

                        state->angle = std::fmod(state->angle, fullTurn);
                    }

                    if (height > 0)
                    {
                        constexpr float halfTurn = 3.14159265f;
                        constexpr float maxTilt = 1.2f;

                        state->tiltAngle +=
                            deltaY / static_cast<float>(height) * halfTurn;

                        state->tiltAngle = std::clamp(
                            state->tiltAngle,
                            -maxTilt,
                            maxTilt
                        );
                    }

                    return 1;
                }

                state->touchActive = false;
                state->activePointerId = -1;
                return 0;
            }

            case AMOTION_EVENT_ACTION_POINTER_UP:
            {
                const int32_t pointerIndex =
                    (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >>
                    AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

                if (pointerIndex >= 0 &&
                    static_cast<size_t>(pointerIndex) < pointerCount)
                {
                    const int32_t liftedId = AMotionEvent_getPointerId(
                        event,
                        static_cast<size_t>(pointerIndex)
                    );

                    if (liftedId == state->pinchPointerId1 ||
                        liftedId == state->pinchPointerId2)
                    {
                        // После отпускания одного пальца завершаем жест.
                        // Для нового вращения нужно снова коснуться экрана.
                        state->pinchActive = false;
                        state->touchActive = false;
                        state->activePointerId = -1;
                    }
                }

                return 1;
            }

            case AMOTION_EVENT_ACTION_UP:
            case AMOTION_EVENT_ACTION_CANCEL:
            {
                state->touchActive = false;
                state->pinchActive = false;
                state->activePointerId = -1;
                state->pinchPointerId1 = -1;
                state->pinchPointerId2 = -1;
                state->lastPinchDistance = 0.0f;

                return 1;
            }

            default:
                return (state->touchActive || state->pinchActive) ? 1 : 0;
        }
    }

    void handleAppCommand(android_app* app, int32_t command)
    {
        auto* state = static_cast<EngineState*>(app->userData);

        if (state == nullptr)
        {
            return;
        }

        switch (command)
        {
            case APP_CMD_START:
                logInfo("ATLAS: приложение запущено.");
                break;

            case APP_CMD_RESUME:
                state->physicsAccumulator = 0.0;
                state->physicsTime = {};
                state->isActive = true;
                state->lastFrameTime = std::chrono::steady_clock::now();
                state->nextFrameTime = state->lastFrameTime;
                logInfo("ATLAS: приложение активно.");
                break;

            case APP_CMD_GAINED_FOCUS:
                state->physicsAccumulator = 0.0;
                state->physicsTime = {};
                state->hasFocus = true;
                state->lastFrameTime = std::chrono::steady_clock::now();
                state->nextFrameTime = state->lastFrameTime;
                state->statsStartTime = state->lastFrameTime;
                state->presentedFrames = 0;
                state->lastPresentedTime = {};
                state->measuredIntervals = 0;
                state->intervalsOver50Ms = 0;
                state->maxIntervalMs = 0.0;
                state->totalWorkMs = 0.0;
                state->maxWorkMs = 0.0;
                updateFpsOverlay(app, -1.0f, 0.0f, 0);
                logInfo("ATLAS: окно получило фокус.");
                break;

            case APP_CMD_LOST_FOCUS:
                state->physicsAccumulator = 0.0;
                state->physicsTime = {};
                state->hasFocus = false;
                state->touchActive = false;
                state->pinchActive = false;
                state->pinchPointerId1 = -1;
                state->pinchPointerId2 = -1;
                state->lastPinchDistance = 0.0f;
                state->activePointerId = -1;
                logInfo("ATLAS: окно потеряло фокус.");
                break;

            case APP_CMD_INIT_WINDOW:
                logInfo("ATLAS: окно создано.");

                if (app->window != nullptr)
                {
                    restartRenderer(app, state);
                }

                break;

            case APP_CMD_WINDOW_REDRAW_NEEDED:
                logInfo("ATLAS: запрошена перерисовка.");
                redraw(app, state);
                break;

            case APP_CMD_WINDOW_RESIZED:
                logInfo("ATLAS: размер окна изменился.");

                if (app->window != nullptr)
                {
                    restartRenderer(app, state);
                }

                break;

            case APP_CMD_PAUSE:
                state->physicsAccumulator = 0.0;
                state->physicsTime = {};
                state->isActive = false;
                logInfo("ATLAS: приложение приостановлено.");
                break;

            case APP_CMD_TERM_WINDOW:
                state->physicsAccumulator = 0.0;
                state->physicsTime = {};
                logInfo("ATLAS: окно закрывается, освобождаем Vulkan.");
                state->renderer.shutdown();
                break;

            case APP_CMD_STOP:
                state->isActive = false;
                state->physicsAccumulator = 0.0;
                state->physicsTime = {};
                logInfo("ATLAS: приложение остановлено.");
                break;

            default:
                break;
        }
    }
}

void android_main(android_app* app)
{
    app_dummy();

    logInfo("ATLAS 0.4b: запуск C++ ядра.");

    EngineState state{};

    app->userData = &state;
    app->onAppCmd = handleAppCommand;
    app->onInputEvent = handleInput;

    constexpr auto frameInterval = std::chrono::nanoseconds(33333333);

    while (app->destroyRequested == 0)
    {
        const bool canAnimate =
            state.isActive &&
            state.hasFocus &&
            app->window != nullptr &&
            state.renderer.isInitialized();

        int timeoutMs = -1;

        if (canAnimate)
        {
            const auto now = std::chrono::steady_clock::now();

            if (now >= state.nextFrameTime)
            {
                timeoutMs = 0;
            }
            else
            {
                const auto remaining = state.nextFrameTime - now;

                // Округляем вверх до миллисекунды, чтобы не крутить
                // пустой цикл перед наступлением времени кадра.
                const auto milliseconds =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        remaining
                    );

                timeoutMs = static_cast<int>(milliseconds.count());

                if (std::chrono::duration_cast<std::chrono::nanoseconds>(
                        milliseconds
                    ) < remaining)
                {
                    ++timeoutMs;
                }
            }
        }

        int events = 0;
        android_poll_source* source = nullptr;

        const int result = ALooper_pollOnce(
            timeoutMs,
            nullptr,
            &events,
            reinterpret_cast<void**>(&source)
        );

        if (result == ALOOPER_POLL_ERROR)
        {
            logError("ATLAS: ошибка ожидания системного события.");
            break;
        }

        if (source != nullptr)
        {
            source->process(app, source);
        }

        if (app->destroyRequested != 0)
        {
            break;
        }

        const bool ready =
            state.isActive &&
            state.hasFocus &&
            app->window != nullptr &&
            state.renderer.isInitialized();

        const auto now = std::chrono::steady_clock::now();

        if (ready && now >= state.nextFrameTime)
        {
            updateAndDraw(app, &state);

            // Планируем следующий кадр относительно предыдущего срока,
            // а не добавляем интервал после завершения обработки.
            state.nextFrameTime += frameInterval;

            const auto afterFrame = std::chrono::steady_clock::now();

            if (state.nextFrameTime <= afterFrame)
            {
                // Если кадр занял слишком много времени, не пытаемся
                // обработать накопившиеся кадры подряд.
                state.nextFrameTime = afterFrame + frameInterval;
            }
        }
    }

    state.renderer.shutdown();

    logInfo("ATLAS 0.4b: завершение работы.");
}
