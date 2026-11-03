#include <android/log.h>
#include <android/native_window.h>
#include <android_native_app_glue.h>

#include <vulkan/vulkan.h>
#include <vector>

#include <cstdint>

namespace
{
    constexpr const char* LOG_TAG = "ATLAS";

    struct EngineState
    {
        bool hasWindow = false;
        bool isActive = false;
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

void inspectVulkan()
{
    logInfo("Начинаем проверку Vulkan.");

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "ATLAS";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 2, 0);
    appInfo.pEngineName = "ATLAS Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 2, 0);

    // Для первой проверки запрашиваем базовую версию Vulkan 1.0.
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo instanceInfo{};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;

    VkInstance instance = VK_NULL_HANDLE;

    VkResult result = vkCreateInstance(
        &instanceInfo,
        nullptr,
        &instance
    );

    if (result != VK_SUCCESS)
    {
        __android_log_print(
            ANDROID_LOG_ERROR,
            LOG_TAG,
            "Vulkan: не удалось создать экземпляр. Код: %d",
            static_cast<int>(result)
        );
        return;
    }

    logInfo("Vulkan: экземпляр успешно создан.");

    uint32_t deviceCount = 0;

    result = vkEnumeratePhysicalDevices(
        instance,
        &deviceCount,
        nullptr
    );

    if (result != VK_SUCCESS || deviceCount == 0)
    {
        __android_log_print(
            ANDROID_LOG_ERROR,
            LOG_TAG,
            "Vulkan: GPU не найден. Код: %d, количество: %u",
            static_cast<int>(result),
            deviceCount
        );

        vkDestroyInstance(instance, nullptr);
        return;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);

    result = vkEnumeratePhysicalDevices(
        instance,
        &deviceCount,
        devices.data()
    );

    if (result != VK_SUCCESS)
    {
        __android_log_print(
            ANDROID_LOG_ERROR,
            LOG_TAG,
            "Vulkan: не удалось получить список GPU. Код: %d",
            static_cast<int>(result)
        );

        vkDestroyInstance(instance, nullptr);
        return;
    }

    __android_log_print(
        ANDROID_LOG_INFO,
        LOG_TAG,
        "Vulkan: найдено устройств: %u",
        deviceCount
    );

    for (uint32_t i = 0; i < deviceCount; ++i)
    {
        VkPhysicalDeviceProperties properties{};

        vkGetPhysicalDeviceProperties(
            devices[i],
            &properties
        );

        __android_log_print(
            ANDROID_LOG_INFO,
            LOG_TAG,
            "GPU %u: %s",
            i,
            properties.deviceName
        );

        __android_log_print(
            ANDROID_LOG_INFO,
            LOG_TAG,
            "Версия Vulkan устройства: %u.%u.%u",
            VK_VERSION_MAJOR(properties.apiVersion),
            VK_VERSION_MINOR(properties.apiVersion),
            VK_VERSION_PATCH(properties.apiVersion)
        );

        __android_log_print(
            ANDROID_LOG_INFO,
            LOG_TAG,
            "Vendor ID: 0x%04X, Device ID: 0x%04X",
            properties.vendorID,
            properties.deviceID
        );
    }

    vkDestroyInstance(instance, nullptr);
    logInfo("Vulkan: проверка завершена, ресурсы освобождены.");
}

    // Диагностическая отрисовка через системный буфер окна.
    // В версии с Vulkan этот код будет заменён.
    void drawTestImage(android_app* app)
    {
        if (app == nullptr || app->window == nullptr)
        {
            return;
        }

        const int geometryResult = ANativeWindow_setBuffersGeometry(
            app->window,
            0,
            0,
            WINDOW_FORMAT_RGBA_8888
        );

        if (geometryResult != 0)
        {
            logError("Не удалось установить формат буфера окна.");
            return;
        }

        ANativeWindow_Buffer buffer{};

        const int lockResult = ANativeWindow_lock(
            app->window,
            &buffer,
            nullptr
        );

        if (lockResult != 0)
        {
            logError("Не удалось получить буфер окна для рисования.");
            return;
        }

        // Заполняем экран тёмно-синим цветом и рисуем светлую полосу.
        // Формат буфера задан выше: RGBA, по одному байту на канал.
        auto* pixels = static_cast<std::uint8_t*>(buffer.bits);

        for (int y = 0; y < buffer.height; ++y)
        {
            for (int x = 0; x < buffer.width; ++x)
            {
                const int pixelIndex = (y * buffer.stride + x) * 4;

                const bool isBand =
                    y > buffer.height * 45 / 100 &&
                    y < buffer.height * 55 / 100;

                pixels[pixelIndex + 0] = isBand ? 55 : 10;   // R
                pixels[pixelIndex + 1] = isBand ? 170 : 20;  // G
                pixels[pixelIndex + 2] = isBand ? 220 : 35;  // B
                pixels[pixelIndex + 3] = 255;                // A
            }
        }

        const int postResult = ANativeWindow_unlockAndPost(app->window);

        if (postResult != 0)
        {
            logError("Не удалось вывести буфер на экран.");
            return;
        }

        logInfo("Диагностический кадр выведен.");
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
                logInfo("Приложение запущено.");
                break;

            case APP_CMD_RESUME:
                state->isActive = true;
                logInfo("Приложение активно.");
                break;

            case APP_CMD_INIT_WINDOW:
                state->hasWindow = app->window != nullptr;
                logInfo("Окно создано.");
                drawTestImage(app);
                break;

            case APP_CMD_WINDOW_REDRAW_NEEDED:
                logInfo("Запрошена перерисовка окна.");
                drawTestImage(app);
                break;

            case APP_CMD_WINDOW_RESIZED:
                logInfo("Размер окна изменился.");
                drawTestImage(app);
                break;

            case APP_CMD_PAUSE:
                state->isActive = false;
                logInfo("Приложение приостановлено.");
                break;

            case APP_CMD_TERM_WINDOW:
                state->hasWindow = false;
                logInfo("Окно закрыто системой.");
                break;

            case APP_CMD_STOP:
                logInfo("Приложение остановлено.");
                break;

            default:
                break;
        }
    }
}

// Точка входа нативного приложения Android.
void android_main(android_app* app)
{
    // Убеждаемся, что код native_app_glue подключён.
    app_dummy();

    logInfo("ATLAS 0.2a: запуск C++ ядра.");

    inspectVulkan();

    EngineState state{};

    app->userData = &state;
    app->onAppCmd = handleAppCommand;

    while (app->destroyRequested == 0)
    {
        int events = 0;
        android_poll_source* source = nullptr;

        // В ATLAS 0.1 непрерывная отрисовка не нужна.
        // Ждём системное событие, не занимая процессор пустым циклом.
        const int result = ALooper_pollOnce(
            -1,
            nullptr,
            &events,
            reinterpret_cast<void**>(&source)
        );

        if (result == ALOOPER_POLL_ERROR)
        {
            logError("Ошибка ожидания системного события.");
            break;
        }

        if (source != nullptr)
        {
            source->process(app, source);
        }
    }

    logInfo("ATLAS 0.1: завершение работы.");
}
