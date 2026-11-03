#include <android/log.h>
#include <android_native_app_glue.h>

#include "renderer/VulkanRenderer.h"

namespace
{
    constexpr const char* LOG_TAG = "ATLAS";

    struct EngineState
    {
        VulkanRenderer renderer;
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

        logInfo("ATLAS: создание Vulkan-рендерера.");

        if (!state->renderer.initialize(
                app->window,
                app->activity->assetManager
            ))
        {
            logError("ATLAS: ошибка запуска Vulkan-рендерера.");
            return false;
        }

        if (!state->renderer.drawFrame())
        {
            logError("ATLAS: первый Vulkan-кадр не выведен.");
            state->renderer.shutdown();
            return false;
        }

        return true;
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

        if (!state->renderer.drawFrame())
        {
            logInfo("ATLAS: повторное создание рендерера.");
            restartRenderer(app, state);
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
                logInfo("ATLAS: приложение активно.");
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
                logInfo("ATLAS: приложение приостановлено.");
                break;

            case APP_CMD_TERM_WINDOW:
                logInfo("ATLAS: окно закрывается, освобождаем Vulkan.");
                state->renderer.shutdown();
                break;

            case APP_CMD_STOP:
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

    while (app->destroyRequested == 0)
    {
        int events = 0;
        android_poll_source* source = nullptr;

        // Постоянная отрисовка пока не нужна.
        // Ожидаем событие Android без пустого цикла.
        const int result = ALooper_pollOnce(
            -1,
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
    }

    state.renderer.shutdown();

    logInfo("ATLAS 0.4b: завершение работы.");
}
