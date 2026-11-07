#include "VulkanRenderer.h"
#include "../scene/TestScene.h"
#include "../scene/MeshLoader.h"
#include "../scene/SceneLoader.h"

#include <android/log.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

namespace
{
    constexpr const char* LOG_TAG = "ATLAS";

    using atlas::Vertex;

    // Матрица 4×4. Элементы хранятся по столбцам,
    // как ожидает GLSL для mat4.
    struct Matrix4
    {
        float values[16];
    };

    static_assert(sizeof(Matrix4) == 64, "Matrix4 должна занимать 64 байта.");

    struct PushConstants
    {
        Matrix4 mvp;

        // Направление к источнику света в координатах модели.
        // Четвёртый компонент — для выравнивания GLSL vec4.
        float lightDirection[4];
    };

    static_assert(
        offsetof(PushConstants, lightDirection) == 64,
        "Неверное смещение lightDirection."
    );

    static_assert(
        sizeof(PushConstants) == 80,
        "PushConstants должен занимать 80 байт."
    );

    Matrix4 multiply(const Matrix4& a, const Matrix4& b)
    {
        Matrix4 result{};

        for (int column = 0; column < 4; ++column)
        {
            for (int row = 0; row < 4; ++row)
            {
                for (int k = 0; k < 4; ++k)
                {
                    result.values[column * 4 + row] +=
                        a.values[k * 4 + row] *
                        b.values[column * 4 + k];
                }
            }
        }

        return result;
    }

    Matrix4 rotationY(float angle)
    {
        const float c = std::cos(angle);
        const float s = std::sin(angle);

        return {{
             c,    0.0f, -s,    0.0f,
             0.0f, 1.0f,  0.0f, 0.0f,
             s,    0.0f,  c,    0.0f,
             0.0f, 0.0f,  0.0f, 1.0f
        }};
    }

    Matrix4 rotationX(float angle)
    {
        const float c = std::cos(angle);
        const float s = std::sin(angle);

        return {{
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, c,    s,    0.0f,
            0.0f, -s,   c,    0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        }};
    }

    Matrix4 translation(float x, float y, float z)
    {
        return {{
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            x,    y,    z,    1.0f
        }};
    }

    Matrix4 uniformScale(float scale)
    {
        return {{
            scale, 0.0f,  0.0f,  0.0f,
            0.0f,  scale, 0.0f,  0.0f,
            0.0f,  0.0f,  scale, 0.0f,
            0.0f,  0.0f,  0.0f,  1.0f
        }};
    }

    Matrix4 cameraView(float distance)
    {
        return {{
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, distance, 1.0f
        }};
    }

    Matrix4 perspective(float aspect)
    {
        const float nearPlane = 0.1f;
        const float farPlane = 20.0f;

        const float scale = std::min(aspect, 1.0f) * 1.8f;
        const float depthScale = farPlane / (farPlane - nearPlane);
        const float depthOffset =
            -(farPlane * nearPlane) / (farPlane - nearPlane);

        return {{
            scale / aspect, 0.0f,  0.0f,        0.0f,
            0.0f,           scale, 0.0f,        0.0f,
            0.0f,           0.0f,  depthScale,  1.0f,
            0.0f,           0.0f,  depthOffset, 0.0f
        }};
    }

    void logInfo(const char* message)
    {
        __android_log_print(
            ANDROID_LOG_INFO,
            LOG_TAG,
            "%s",
            message
        );
    }

    bool checkResult(VkResult result, const char* operation)
    {
        if (result == VK_SUCCESS)
        {
            return true;
        }

        __android_log_print(
            ANDROID_LOG_ERROR,
            LOG_TAG,
            "%s: ошибка VkResult=%d",
            operation,
            static_cast<int>(result)
        );

        return false;
    }

    bool supportsSwapchainExtension(VkPhysicalDevice device)
    {
        uint32_t count = 0;

        VkResult result = vkEnumerateDeviceExtensionProperties(
            device,
            nullptr,
            &count,
            nullptr
        );

        if (result != VK_SUCCESS || count == 0)
        {
            return false;
        }

        std::vector<VkExtensionProperties> extensions(count);

        result = vkEnumerateDeviceExtensionProperties(
            device,
            nullptr,
            &count,
            extensions.data()
        );

        if (result != VK_SUCCESS)
        {
            return false;
        }

        for (uint32_t i = 0; i < count; ++i)
        {
            if (std::strcmp(
                    extensions[i].extensionName,
                    VK_KHR_SWAPCHAIN_EXTENSION_NAME
                ) == 0)
            {
                return true;
            }
        }

        return false;
    }

    VkCompositeAlphaFlagBitsKHR chooseCompositeAlpha(
        VkCompositeAlphaFlagsKHR supported
    )
    {
        const VkCompositeAlphaFlagBitsKHR candidates[] = {
            VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
            VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR
        };

        for (VkCompositeAlphaFlagBitsKHR candidate : candidates)
        {
            if ((supported & candidate) != 0)
            {
                return candidate;
            }
        }

        return static_cast<VkCompositeAlphaFlagBitsKHR>(0);
    }
}

VulkanRenderer::~VulkanRenderer()
{
    shutdown();
}

bool VulkanRenderer::isInitialized() const
{
    return initialized_;
}

bool VulkanRenderer::isSwapchainOutOfDate() const
{
    return swapchainOutOfDate_;
}

bool VulkanRenderer::initialize(
    ANativeWindow* window,
    AAssetManager* assetManager
)
{
    shutdown();

    if (window == nullptr)
    {
        logInfo("Vulkan: окно отсутствует.");
        return false;
    }

    if (assetManager == nullptr)
    {
        __android_log_print(
            ANDROID_LOG_ERROR,
            LOG_TAG,
            "Vulkan: AssetManager отсутствует."
        );
        return false;
    }

    if (!atlas::loadScene(
            assetManager,
            "scenes/test.atlasscene",
            sceneObjects_
        ) ||
        !createInstance() ||
        !createSurface(window) ||
        !selectPhysicalDevice() ||
        !createDevice() ||
        !createSwapchain() ||
        !createDepthResources() ||
        !createRenderPass() ||
        !createGraphicsPipeline(assetManager) ||
        !createImageViews() ||
        !createFramebuffers() ||
        !createVertexBuffer(assetManager) ||
        !createCommandResources() ||
        !createSyncObjects())
    {
        __android_log_print(
            ANDROID_LOG_ERROR,
            LOG_TAG,
            "Vulkan: инициализация не завершена."
        );

        shutdown();
        return false;
    }

    initialized_ = true;

    logInfo("Vulkan: рендерер готов.");
    return true;
}

bool VulkanRenderer::createInstance()
{
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "ATLAS";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 3, 0);
    appInfo.pEngineName = "ATLAS Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 3, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    const char* extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
    };

    VkInstanceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pApplicationInfo = &appInfo;
    info.enabledExtensionCount = 2;
    info.ppEnabledExtensionNames = extensions;

    return checkResult(
        vkCreateInstance(&info, nullptr, &instance_),
        "vkCreateInstance"
    );
}

bool VulkanRenderer::createSurface(ANativeWindow* window)
{
    VkAndroidSurfaceCreateInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    info.window = window;

    return checkResult(
        vkCreateAndroidSurfaceKHR(
            instance_,
            &info,
            nullptr,
            &surface_
        ),
        "vkCreateAndroidSurfaceKHR"
    );
}

bool VulkanRenderer::selectPhysicalDevice()
{
    uint32_t deviceCount = 0;

    VkResult result = vkEnumeratePhysicalDevices(
        instance_,
        &deviceCount,
        nullptr
    );

    if (!checkResult(result, "vkEnumeratePhysicalDevices") ||
        deviceCount == 0)
    {
        logInfo("Vulkan: доступные устройства не найдены.");
        return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);

    result = vkEnumeratePhysicalDevices(
        instance_,
        &deviceCount,
        devices.data()
    );

    if (!checkResult(result, "Получение списка GPU"))
    {
        return false;
    }

    for (uint32_t deviceIndex = 0;
         deviceIndex < deviceCount;
         ++deviceIndex)
    {
        VkPhysicalDevice candidate = devices[deviceIndex];

        if (!supportsSwapchainExtension(candidate))
        {
            continue;
        }

        uint32_t queueCount = 0;

        vkGetPhysicalDeviceQueueFamilyProperties(
            candidate,
            &queueCount,
            nullptr
        );

        if (queueCount == 0)
        {
            continue;
        }

        std::vector<VkQueueFamilyProperties> queues(queueCount);

        vkGetPhysicalDeviceQueueFamilyProperties(
            candidate,
            &queueCount,
            queues.data()
        );

        for (uint32_t queueIndex = 0;
             queueIndex < queueCount;
             ++queueIndex)
        {
            const bool supportsGraphics =
                (queues[queueIndex].queueFlags &
                 VK_QUEUE_GRAPHICS_BIT) != 0;

            if (!supportsGraphics || queues[queueIndex].queueCount == 0)
            {
                continue;
            }

            VkBool32 supportsPresent = VK_FALSE;

            result = vkGetPhysicalDeviceSurfaceSupportKHR(
                candidate,
                queueIndex,
                surface_,
                &supportsPresent
            );

            if (result != VK_SUCCESS ||
                supportsPresent != VK_TRUE)
            {
                continue;
            }

            physicalDevice_ = candidate;
            queueFamilyIndex_ = queueIndex;

            VkPhysicalDeviceProperties properties{};
            vkGetPhysicalDeviceProperties(
                physicalDevice_,
                &properties
            );

            __android_log_print(
                ANDROID_LOG_INFO,
                LOG_TAG,
                "Vulkan: выбран GPU '%s', очередь %u.",
                properties.deviceName,
                queueFamilyIndex_
            );

            return true;
        }
    }

    __android_log_print(
        ANDROID_LOG_ERROR,
        LOG_TAG,
        "Vulkan: подходящий GPU или общая очередь не найдены."
    );

    return false;
}

bool VulkanRenderer::createDevice()
{
    const float queuePriority = 1.0f;

    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = queueFamilyIndex_;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &queuePriority;

    const char* extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    VkDeviceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    info.queueCreateInfoCount = 1;
    info.pQueueCreateInfos = &queueInfo;
    info.enabledExtensionCount = 1;
    info.ppEnabledExtensionNames = extensions;

    if (!checkResult(
            vkCreateDevice(
                physicalDevice_,
                &info,
                nullptr,
                &device_
            ),
            "vkCreateDevice"
        ))
    {
        return false;
    }

    vkGetDeviceQueue(
        device_,
        queueFamilyIndex_,
        0,
        &queue_
    );

    return queue_ != VK_NULL_HANDLE;
}

bool VulkanRenderer::createSwapchain()
{
    VkSurfaceCapabilitiesKHR capabilities{};

    if (!checkResult(
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
                physicalDevice_,
                surface_,
                &capabilities
            ),
            "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"
        ))
    {
        return false;
    }

    if ((capabilities.supportedUsageFlags &
         VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0)
    {
        __android_log_print(
            ANDROID_LOG_ERROR,
            LOG_TAG,
            "Vulkan: поверхность не поддерживает color attachment."
        );
        return false;
    }

    uint32_t formatCount = 0;

    if (!checkResult(
            vkGetPhysicalDeviceSurfaceFormatsKHR(
                physicalDevice_,
                surface_,
                &formatCount,
                nullptr
            ),
            "Получение количества форматов"
        ) || formatCount == 0)
    {
        return false;
    }

    std::vector<VkSurfaceFormatKHR> formats(formatCount);

    if (!checkResult(
            vkGetPhysicalDeviceSurfaceFormatsKHR(
                physicalDevice_,
                surface_,
                &formatCount,
                formats.data()
            ),
            "Получение форматов поверхности"
        ))
    {
        return false;
    }

    VkSurfaceFormatKHR selectedFormat = formats[0];

    if (formatCount == 1 &&
        formats[0].format == VK_FORMAT_UNDEFINED)
    {
        selectedFormat.format = VK_FORMAT_R8G8B8A8_UNORM;
    }
    else
    {
        for (uint32_t i = 0; i < formatCount; ++i)
        {
            if (formats[i].format == VK_FORMAT_R8G8B8A8_UNORM ||
                formats[i].format == VK_FORMAT_B8G8R8A8_UNORM)
            {
                selectedFormat = formats[i];
                break;
            }
        }
    }

    swapchainFormat_ = selectedFormat.format;

    if (capabilities.currentExtent.width !=
        std::numeric_limits<uint32_t>::max())
    {
        swapchainExtent_ = capabilities.currentExtent;
    }
    else
    {
        // Если поверхность позволяет выбрать размер, используем
        // минимально допустимый. Обычно Android сообщает currentExtent.
        swapchainExtent_ = capabilities.minImageExtent;
    }

    if (swapchainExtent_.width == 0 ||
        swapchainExtent_.height == 0)
    {
        __android_log_print(
            ANDROID_LOG_ERROR,
            LOG_TAG,
            "Vulkan: размер поверхности равен нулю."
        );
        return false;
    }

    uint32_t imageCount = capabilities.minImageCount + 1;

    if (capabilities.maxImageCount > 0 &&
        imageCount > capabilities.maxImageCount)
    {
        imageCount = capabilities.maxImageCount;
    }

    const VkCompositeAlphaFlagBitsKHR compositeAlpha =
        chooseCompositeAlpha(capabilities.supportedCompositeAlpha);

    if (compositeAlpha == 0)
    {
        __android_log_print(
            ANDROID_LOG_ERROR,
            LOG_TAG,
            "Vulkan: подходящий composite alpha не найден."
        );
        return false;
    }

    VkSwapchainCreateInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface = surface_;
    info.minImageCount = imageCount;
    info.imageFormat = selectedFormat.format;
    info.imageColorSpace = selectedFormat.colorSpace;
    info.imageExtent = swapchainExtent_;
    info.imageArrayLayers = 1;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if ((capabilities.supportedTransforms &
         VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) == 0)
    {
        __android_log_print(
            ANDROID_LOG_ERROR,
            LOG_TAG,
            "Vulkan: identity transform не поддерживается. "
            "currentTransform=%u, supportedTransforms=%u",
            static_cast<unsigned>(capabilities.currentTransform),
            static_cast<unsigned>(capabilities.supportedTransforms)
        );

        return false;
    }

    surfaceTransform_ = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    info.preTransform = surfaceTransform_;

    __android_log_print(
        ANDROID_LOG_INFO,
        LOG_TAG,
        "Vulkan: currentTransform=%u, выбран identity, swapchain=%ux%u",
        static_cast<unsigned>(capabilities.currentTransform),
        swapchainExtent_.width,
        swapchainExtent_.height
    );


    info.compositeAlpha = compositeAlpha;

    // FIFO обязателен для реализации Vulkan с поддержкой swapchain.
    info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    info.clipped = VK_TRUE;
    info.oldSwapchain = VK_NULL_HANDLE;

    if (!checkResult(
            vkCreateSwapchainKHR(
                device_,
                &info,
                nullptr,
                &swapchain_
            ),
            "vkCreateSwapchainKHR"
        ))
    {
        return false;
    }

    uint32_t actualImageCount = 0;

    if (!checkResult(
            vkGetSwapchainImagesKHR(
                device_,
                swapchain_,
                &actualImageCount,
                nullptr
            ),
            "Получение количества изображений swapchain"
        ) || actualImageCount == 0)
    {
        return false;
    }

    images_.resize(actualImageCount);

    if (!checkResult(
            vkGetSwapchainImagesKHR(
                device_,
                swapchain_,
                &actualImageCount,
                images_.data()
            ),
            "Получение изображений swapchain"
        ))
    {
        return false;
    }

    images_.resize(actualImageCount);

    __android_log_print(
        ANDROID_LOG_INFO,
        LOG_TAG,
        "Vulkan: swapchain %ux%u, изображений: %u.",
        swapchainExtent_.width,
        swapchainExtent_.height,
        actualImageCount
    );

    return true;
}

bool VulkanRenderer::createDepthResources()
{
    // Выбираем формат, который устройство поддерживает
    // для использования в качестве буфера глубины.
    const VkFormat candidates[] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D16_UNORM
    };

    depthFormat_ = VK_FORMAT_UNDEFINED;

    for (VkFormat candidate : candidates)
    {
        VkFormatProperties properties{};

        vkGetPhysicalDeviceFormatProperties(
            physicalDevice_,
            candidate,
            &properties
        );

        if ((properties.optimalTilingFeatures &
             VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)
        {
            depthFormat_ = candidate;
            break;
        }
    }

    if (depthFormat_ == VK_FORMAT_UNDEFINED)
    {
        __android_log_print(
            ANDROID_LOG_ERROR,
            LOG_TAG,
            "Vulkan: подходящий формат глубины не найден."
        );
        return false;
    }

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = depthFormat_;
    imageInfo.extent.width = swapchainExtent_.width;
    imageInfo.extent.height = swapchainExtent_.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (!checkResult(
            vkCreateImage(
                device_,
                &imageInfo,
                nullptr,
                &depthImage_
            ),
            "vkCreateImage для глубины"
        ))
    {
        return false;
    }

    VkMemoryRequirements requirements{};

    vkGetImageMemoryRequirements(
        device_,
        depthImage_,
        &requirements
    );

    VkPhysicalDeviceMemoryProperties memoryProperties{};

    vkGetPhysicalDeviceMemoryProperties(
        physicalDevice_,
        &memoryProperties
    );

    uint32_t memoryTypeIndex = UINT32_MAX;

    // Предпочитаем память устройства.
    for (uint32_t i = 0;
         i < memoryProperties.memoryTypeCount;
         ++i)
    {
        const bool supported =
            (requirements.memoryTypeBits & (1u << i)) != 0;

        const bool deviceLocal =
            (memoryProperties.memoryTypes[i].propertyFlags &
             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0;

        if (supported && deviceLocal)
        {
            memoryTypeIndex = i;
            break;
        }
    }

    // Если такой тип не найден, используем любой допустимый.
    if (memoryTypeIndex == UINT32_MAX)
    {
        for (uint32_t i = 0;
             i < memoryProperties.memoryTypeCount;
             ++i)
        {
            if ((requirements.memoryTypeBits & (1u << i)) != 0)
            {
                memoryTypeIndex = i;
                break;
            }
        }
    }

    if (memoryTypeIndex == UINT32_MAX)
    {
        __android_log_print(
            ANDROID_LOG_ERROR,
            LOG_TAG,
            "Vulkan: тип памяти для буфера глубины не найден."
        );
        return false;
    }

    VkMemoryAllocateInfo allocationInfo{};
    allocationInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocationInfo.allocationSize = requirements.size;
    allocationInfo.memoryTypeIndex = memoryTypeIndex;

    if (!checkResult(
            vkAllocateMemory(
                device_,
                &allocationInfo,
                nullptr,
                &depthMemory_
            ),
            "vkAllocateMemory для глубины"
        ))
    {
        return false;
    }

    if (!checkResult(
            vkBindImageMemory(
                device_,
                depthImage_,
                depthMemory_,
                0
            ),
            "vkBindImageMemory для глубины"
        ))
    {
        return false;
    }

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = depthImage_;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = depthFormat_;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (!checkResult(
            vkCreateImageView(
                device_,
                &viewInfo,
                nullptr,
                &depthImageView_
            ),
            "vkCreateImageView для глубины"
        ))
    {
        return false;
    }

    __android_log_print(
        ANDROID_LOG_INFO,
        LOG_TAG,
        "Vulkan: буфер глубины создан, формат %d.",
        static_cast<int>(depthFormat_)
    );

    return true;
}

bool VulkanRenderer::createRenderPass()
{
    VkAttachmentDescription attachments[2]{};

    // 0: цветное изображение swapchain.
    attachments[0].format = swapchainFormat_;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // 1: буфер глубины.
    attachments[1].format = depthFormat_;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorReference{};
    colorReference.attachment = 0;
    colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthReference{};
    depthReference.attachment = 1;
    depthReference.layout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorReference;
    subpass.pDepthStencilAttachment = &depthReference;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependency.dstStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = 2;
    info.pAttachments = attachments;
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = 1;
    info.pDependencies = &dependency;

    return checkResult(
        vkCreateRenderPass(
            device_,
            &info,
            nullptr,
            &renderPass_
        ),
        "vkCreateRenderPass"
    );
}

bool VulkanRenderer::loadShaderModule(
    AAssetManager* assetManager,
    const char* path,
    VkShaderModule* shaderModule
)
{
    if (assetManager == nullptr ||
        path == nullptr ||
        shaderModule == nullptr)
    {
        return false;
    }

    *shaderModule = VK_NULL_HANDLE;

    AAsset* asset = AAssetManager_open(
        assetManager,
        path,
        AASSET_MODE_BUFFER
    );

    if (asset == nullptr)
    {
        __android_log_print(
            ANDROID_LOG_ERROR,
            LOG_TAG,
            "Не удалось открыть шейдер: %s",
            path
        );
        return false;
    }

    const off_t length = AAsset_getLength(asset);

    if (length <= 0 || length % 4 != 0)
    {
        __android_log_print(
            ANDROID_LOG_ERROR,
            LOG_TAG,
            "Некорректный размер шейдера %s: %lld",
            path,
            static_cast<long long>(length)
        );

        AAsset_close(asset);
        return false;
    }

    // SPIR-V состоит из 32-битных слов.
    std::vector<uint32_t> code(
        static_cast<size_t>(length) / sizeof(uint32_t)
    );

    auto* destination = reinterpret_cast<unsigned char*>(code.data());
    size_t totalRead = 0;
    const size_t expectedSize = static_cast<size_t>(length);

    while (totalRead < expectedSize)
    {
        const int bytesRead = AAsset_read(
            asset,
            destination + totalRead,
            expectedSize - totalRead
        );

        if (bytesRead <= 0)
        {
            __android_log_print(
                ANDROID_LOG_ERROR,
                LOG_TAG,
                "Не удалось полностью прочитать шейдер: %s",
                path
            );

            AAsset_close(asset);
            return false;
        }

        totalRead += static_cast<size_t>(bytesRead);
    }

    AAsset_close(asset);

    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = expectedSize;
    info.pCode = code.data();

    const VkResult result = vkCreateShaderModule(
        device_,
        &info,
        nullptr,
        shaderModule
    );

    if (!checkResult(result, "vkCreateShaderModule"))
    {
        return false;
    }

    __android_log_print(
        ANDROID_LOG_INFO,
        LOG_TAG,
        "Шейдер загружен: %s",
        path
    );

    return true;
}

bool VulkanRenderer::createGraphicsPipeline(
    AAssetManager* assetManager
)
{
    VkShaderModule vertexShader = VK_NULL_HANDLE;
    VkShaderModule fragmentShader = VK_NULL_HANDLE;

    if (!loadShaderModule(
            assetManager,
            "shaders/triangle.vert.spv",
            &vertexShader
        ))
    {
        return false;
    }

    if (!loadShaderModule(
            assetManager,
            "shaders/triangle.frag.spv",
            &fragmentShader
        ))
    {
        vkDestroyShaderModule(device_, vertexShader, nullptr);
        return false;
    }

    VkPipelineShaderStageCreateInfo vertexStage{};
    vertexStage.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertexStage.module = vertexShader;
    vertexStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragmentStage{};
    fragmentStage.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragmentStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragmentStage.module = fragmentShader;
    fragmentStage.pName = "main";

    const VkPipelineShaderStageCreateInfo stages[] = {
        vertexStage,
        fragmentStage
    };

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(Vertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attributes[3]{};

    // location = 0: позиция из трёх чисел float.
    attributes[0].location = 0;
    attributes[0].binding = 0;
    attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[0].offset = offsetof(Vertex, position);

    // location = 1: цвет из трёх чисел float.
    attributes[1].location = 1;
    attributes[1].binding = 0;
    attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[1].offset = offsetof(Vertex, color);

    // location = 2: нормаль из трёх чисел float.
    attributes[2].location = 2;
    attributes[2].binding = 0;
    attributes[2].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[2].offset = offsetof(Vertex, normal);

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = 3;
    vertexInput.pVertexAttributeDescriptions = attributes;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapchainExtent_.width);
    viewport.height = static_cast<float>(swapchainExtent_.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapchainExtent_;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;

    // Пока не отбрасываем ни одну сторону треугольника.
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorAttachment{};
    colorAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;
    colorAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType =
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorAttachment;

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    // Матрица MVP и направление света.
    pushConstantRange.size = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstantRange;

    VkResult result = vkCreatePipelineLayout(
        device_,
        &layoutInfo,
        nullptr,
        &pipelineLayout_
    );

    if (result != VK_SUCCESS)
    {
        checkResult(result, "vkCreatePipelineLayout");

        vkDestroyShaderModule(device_, fragmentShader, nullptr);
        vkDestroyShaderModule(device_, vertexShader, nullptr);
        return false;
    }

    VkPipelineDepthStencilStateCreateInfo depthState{};
    depthState.sType =
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

    depthState.depthTestEnable = VK_TRUE;
    depthState.depthWriteEnable = VK_TRUE;
    depthState.depthCompareOp = VK_COMPARE_OP_LESS;

    depthState.depthBoundsTestEnable = VK_FALSE;
    depthState.stencilTestEnable = VK_FALSE;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthState;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = pipelineLayout_;
    pipelineInfo.renderPass = renderPass_;
    pipelineInfo.subpass = 0;

    result = vkCreateGraphicsPipelines(
        device_,
        VK_NULL_HANDLE,
        1,
        &pipelineInfo,
        nullptr,
        &graphicsPipeline_
    );

    // После создания конвейера отдельные модули шейдеров
    // больше не нужны.
    vkDestroyShaderModule(device_, fragmentShader, nullptr);
    vkDestroyShaderModule(device_, vertexShader, nullptr);

    if (!checkResult(result, "vkCreateGraphicsPipelines"))
    {
        // pipelineLayout_ будет освобождён в shutdown().
        return false;
    }

    logInfo("Vulkan: графический конвейер создан.");
    return true;
}

bool VulkanRenderer::createImageViews()
{
    imageViews_.resize(images_.size(), VK_NULL_HANDLE);

    for (size_t i = 0; i < images_.size(); ++i)
    {
        VkImageViewCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.image = images_[i];
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = swapchainFormat_;

        info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        info.subresourceRange.baseMipLevel = 0;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.baseArrayLayer = 0;
        info.subresourceRange.layerCount = 1;

        if (!checkResult(
                vkCreateImageView(
                    device_,
                    &info,
                    nullptr,
                    &imageViews_[i]
                ),
                "vkCreateImageView"
            ))
        {
            return false;
        }
    }

    return true;
}

bool VulkanRenderer::createFramebuffers()
{
    framebuffers_.resize(imageViews_.size(), VK_NULL_HANDLE);

    for (size_t i = 0; i < imageViews_.size(); ++i)
    {
        VkFramebufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass = renderPass_;
        const VkImageView attachments[] = {
            imageViews_[i],
            depthImageView_
        };

        info.attachmentCount = 2;
        info.pAttachments = attachments;
        info.width = swapchainExtent_.width;
        info.height = swapchainExtent_.height;
        info.layers = 1;

        if (!checkResult(
                vkCreateFramebuffer(
                    device_,
                    &info,
                    nullptr,
                    &framebuffers_[i]
                ),
                "vkCreateFramebuffer"
            ))
        {
            return false;
        }
    }

    return true;
}

bool VulkanRenderer::createVertexBuffer(
    AAssetManager* assetManager
)
{
    // Ограничение для текущего прототипа.
    // Это не предел будущей системы потоковой загрузки.
    constexpr size_t MAX_TOTAL_VERTICES = 1000000;

    std::vector<Vertex> vertices;
    std::vector<MeshRange> ranges;

    for (const atlas::SceneObject& object : sceneObjects_)
    {
        bool alreadyLoaded = false;

        for (const MeshRange& range : ranges)
        {
            if (range.name == object.meshName)
            {
                alreadyLoaded = true;
                break;
            }
        }

        if (alreadyLoaded)
        {
            continue;
        }

        const std::string path =
            "models/" + object.meshName + ".atlasmesh";

        std::vector<Vertex> modelVertices;

        if (!atlas::loadMesh(
                assetManager,
                path.c_str(),
                modelVertices
            ))
        {
            return false;
        }

        if (modelVertices.size() > MAX_TOTAL_VERTICES - vertices.size())
        {
            __android_log_print(
                ANDROID_LOG_ERROR,
                LOG_TAG,
                "Vulkan: превышен общий лимит вершин сцены."
            );
            return false;
        }

        MeshRange range{};
        range.name = object.meshName;
        range.firstVertex = static_cast<uint32_t>(vertices.size());
        range.vertexCount = static_cast<uint32_t>(modelVertices.size());

        vertices.insert(
            vertices.end(),
            modelVertices.begin(),
            modelVertices.end()
        );

        ranges.push_back(range);
    }

    if (vertices.empty())
    {
        __android_log_print(
            ANDROID_LOG_ERROR,
            LOG_TAG,
            "Vulkan: в сцене нет вершин для загрузки."
        );
        return false;
    }

    const VkDeviceSize bufferSize =
        static_cast<VkDeviceSize>(vertices.size() * sizeof(Vertex));

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (!checkResult(
            vkCreateBuffer(
                device_,
                &bufferInfo,
                nullptr,
                &vertexBuffer_
            ),
            "vkCreateBuffer"
        ))
    {
        return false;
    }

    VkMemoryRequirements requirements{};

    vkGetBufferMemoryRequirements(
        device_,
        vertexBuffer_,
        &requirements
    );

    VkPhysicalDeviceMemoryProperties memoryProperties{};

    vkGetPhysicalDeviceMemoryProperties(
        physicalDevice_,
        &memoryProperties
    );

    const VkMemoryPropertyFlags requiredFlags =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    uint32_t memoryTypeIndex = UINT32_MAX;

    for (uint32_t i = 0;
         i < memoryProperties.memoryTypeCount;
         ++i)
    {
        const bool supportedByBuffer =
            (requirements.memoryTypeBits & (1u << i)) != 0;

        const bool hasRequiredFlags =
            (memoryProperties.memoryTypes[i].propertyFlags &
             requiredFlags) == requiredFlags;

        if (supportedByBuffer && hasRequiredFlags)
        {
            memoryTypeIndex = i;
            break;
        }
    }

    if (memoryTypeIndex == UINT32_MAX)
    {
        __android_log_print(
            ANDROID_LOG_ERROR,
            LOG_TAG,
            "Vulkan: подходящий тип памяти для вершин не найден."
        );
        return false;
    }

    VkMemoryAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = requirements.size;
    allocateInfo.memoryTypeIndex = memoryTypeIndex;

    if (!checkResult(
            vkAllocateMemory(
                device_,
                &allocateInfo,
                nullptr,
                &vertexBufferMemory_
            ),
            "vkAllocateMemory для вершин"
        ))
    {
        return false;
    }

    if (!checkResult(
            vkBindBufferMemory(
                device_,
                vertexBuffer_,
                vertexBufferMemory_,
                0
            ),
            "vkBindBufferMemory"
        ))
    {
        return false;
    }

    void* mappedMemory = nullptr;

    if (!checkResult(
            vkMapMemory(
                device_,
                vertexBufferMemory_,
                0,
                bufferSize,
                0,
                &mappedMemory
            ),
            "vkMapMemory для вершин"
        ))
    {
        return false;
    }

    std::memcpy(
        mappedMemory,
        vertices.data(),
        static_cast<size_t>(bufferSize)
    );

    vkUnmapMemory(device_, vertexBufferMemory_);
    meshRanges_ = std::move(ranges);

    __android_log_print(
        ANDROID_LOG_INFO,
        LOG_TAG,
        "Vulkan: уникальных моделей: %u, всего вершин: %u",
        static_cast<unsigned>(meshRanges_.size()),
        static_cast<unsigned>(vertices.size())
    );

    logInfo("Vulkan: буфер вершин создан и заполнен.");
    return true;
}

bool VulkanRenderer::createCommandResources()
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndex_;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (!checkResult(
            vkCreateCommandPool(
                device_,
                &poolInfo,
                nullptr,
                &commandPool_
            ),
            "vkCreateCommandPool"
        ))
    {
        return false;
    }

    commandBuffers_.resize(framebuffers_.size(), VK_NULL_HANDLE);

    VkCommandBufferAllocateInfo allocateInfo{};
    allocateInfo.sType =
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = commandPool_;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount =
        static_cast<uint32_t>(commandBuffers_.size());

    if (!checkResult(
            vkAllocateCommandBuffers(
                device_,
                &allocateInfo,
                commandBuffers_.data()
            ),
            "vkAllocateCommandBuffers"
        ))
    {
        return false;
    }

    return recordCommandBuffers(0.55f, -0.35f, 3.0f);
}

bool VulkanRenderer::recordCommandBuffers(
    float angleY,
    float angleX,
    float cameraDistance
)
{
    for (size_t i = 0; i < commandBuffers_.size(); ++i)
    {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (!checkResult(
                vkBeginCommandBuffer(commandBuffers_[i], &beginInfo),
                "vkBeginCommandBuffer"
            ))
        {
            return false;
        }

        VkClearValue clearValues[2]{};

        // Цвет фона.
        clearValues[0].color.float32[0] = 0.02f;
        clearValues[0].color.float32[1] = 0.22f;
        clearValues[0].color.float32[2] = 0.12f;
        clearValues[0].color.float32[3] = 1.0f;

        // Самая дальняя глубина в диапазоне Vulkan.
        clearValues[1].depthStencil.depth = 1.0f;
        clearValues[1].depthStencil.stencil = 0;

        VkRenderPassBeginInfo renderInfo{};
        renderInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderInfo.renderPass = renderPass_;
        renderInfo.framebuffer = framebuffers_[i];
        renderInfo.renderArea.offset = {0, 0};
        renderInfo.renderArea.extent = swapchainExtent_;
        renderInfo.clearValueCount = 2;
        renderInfo.pClearValues = clearValues;

        vkCmdBeginRenderPass(
            commandBuffers_[i],
            &renderInfo,
            VK_SUBPASS_CONTENTS_INLINE
        );

        vkCmdBindPipeline(
            commandBuffers_[i],
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            graphicsPipeline_
        );

        const VkBuffer vertexBuffers[] = {vertexBuffer_};
        const VkDeviceSize offsets[] = {0};

        vkCmdBindVertexBuffers(
            commandBuffers_[i],
            0,
            1,
            vertexBuffers,
            offsets
        );

        const float width = static_cast<float>(swapchainExtent_.width);
        const float height = static_cast<float>(swapchainExtent_.height);
        const float aspect = width / height;

        // Сначала поворачиваем сцену относительно наблюдателя,
        // затем размещаем её перед камерой.
        const Matrix4 view = multiply(
            cameraView(cameraDistance),
            multiply(
                rotationX(angleX),
                rotationY(angleY)
            )
        );
        const Matrix4 projection = perspective(aspect);

        for (const atlas::SceneObject& object : sceneObjects_)
        {
            // Собственный поворот объекта не зависит от камеры.
            const Matrix4 rotation = rotationY(object.rotationOffset);

            const Matrix4 position = translation(
                object.x,
                object.y,
                object.z
            );

            const Matrix4 model = multiply(
                position,
                multiply(rotation, uniformScale(object.scale))
            );

            const Matrix4 mvp = multiply(
                projection,
                multiply(view, model)
            );

            // Направление к источнику света в координатах сцены.
            // Его нормализация выполняется в шейдере.
            constexpr float lightX = 0.4f;
            constexpr float lightY = 0.8f;
            constexpr float lightZ = -0.6f;

            const float c = std::cos(object.rotationOffset);
            const float s = std::sin(object.rotationOffset);

            PushConstants push{};
            push.mvp = mvp;

            // Обратный поворот переводит свет из координат сцены
            // в локальные координаты модели.
            push.lightDirection[0] = c * lightX - s * lightZ;
            push.lightDirection[1] = lightY;
            push.lightDirection[2] = s * lightX + c * lightZ;
            push.lightDirection[3] = 0.0f;

            vkCmdPushConstants(
                commandBuffers_[i],
                pipelineLayout_,
                VK_SHADER_STAGE_VERTEX_BIT,
                0,
                sizeof(PushConstants),
                &push
            );

            const MeshRange* selectedMesh = nullptr;

            for (const MeshRange& range : meshRanges_)
            {
                if (range.name == object.meshName)
                {
                    selectedMesh = &range;
                    break;
                }
            }

            if (selectedMesh == nullptr)
            {
                __android_log_print(
                    ANDROID_LOG_ERROR,
                    LOG_TAG,
                    "Vulkan: модель объекта не найдена: %s",
                    object.meshName.c_str()
                );

                return false;
            }

            vkCmdDraw(
                commandBuffers_[i],
                selectedMesh->vertexCount,
                1,
                selectedMesh->firstVertex,
                0
            );
        }

        vkCmdEndRenderPass(commandBuffers_[i]);

        if (!checkResult(
                vkEndCommandBuffer(commandBuffers_[i]),
                "vkEndCommandBuffer"
            ))
        {
            return false;
        }
    }

    return true;
}

bool VulkanRenderer::updateRotation(
    float angleY,
    float angleX,
    float cameraDistance
)
{
    if (!initialized_)
    {
        return false;
    }

    // В текущей версии drawFrame() завершает работу очереди
    // через vkQueueWaitIdle(), поэтому предыдущие команды уже
    // не выполняются. Для многокадрового рендера это изменится.
    if (!checkResult(
            vkResetCommandPool(device_, commandPool_, 0),
            "vkResetCommandPool"
        ))
    {
        return false;
    }

    return recordCommandBuffers(angleY, angleX, cameraDistance);
}

bool VulkanRenderer::createSyncObjects()
{
    VkSemaphoreCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    if (!checkResult(
            vkCreateSemaphore(
                device_,
                &info,
                nullptr,
                &imageAvailable_
            ),
            "Создание imageAvailable"
        ))
    {
        return false;
    }

    if (!checkResult(
            vkCreateSemaphore(
                device_,
                &info,
                nullptr,
                &renderFinished_
            ),
            "Создание renderFinished"
        ))
    {
        return false;
    }

    return true;
}

bool VulkanRenderer::drawFrame()
{
    swapchainOutOfDate_ = false;

    if (!initialized_)
    {
        __android_log_print(
            ANDROID_LOG_ERROR,
            LOG_TAG,
            "Vulkan: попытка рисования до инициализации."
        );
        return false;
    }

    uint32_t imageIndex = 0;

    VkResult result = vkAcquireNextImageKHR(
        device_,
        swapchain_,
        UINT64_MAX,
        imageAvailable_,
        VK_NULL_HANDLE,
        &imageIndex
    );

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        swapchainOutOfDate_ = true;
        logInfo("Vulkan: swapchain устарел при получении изображения.");
        return false;
    }

    const bool acquireSuboptimal = result == VK_SUBOPTIMAL_KHR;

    if (result != VK_SUCCESS && !acquireSuboptimal)
    {
        checkResult(result, "vkAcquireNextImageKHR");
        return false;
    }

    VkPipelineStageFlags waitStage =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &imageAvailable_;
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers_[imageIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &renderFinished_;

    result = vkQueueSubmit(
        queue_,
        1,
        &submitInfo,
        VK_NULL_HANDLE
    );

    if (!checkResult(result, "vkQueueSubmit"))
    {
        return false;
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinished_;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain_;
    presentInfo.pImageIndices = &imageIndex;

    const VkResult presentResult =
        vkQueuePresentKHR(queue_, &presentInfo);

    // Текущая диагностическая версия обрабатывает кадры
    // последовательно. Для игрового цикла это будет заменено.
    if (!checkResult(vkQueueWaitIdle(queue_), "vkQueueWaitIdle"))
    {
        return false;
    }

    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
        swapchainOutOfDate_ = true;
        logInfo("Vulkan: swapchain устарел при выводе кадра.");
        return false;
    }

    if (presentResult != VK_SUBOPTIMAL_KHR &&
        !checkResult(presentResult, "vkQueuePresentKHR"))
    {
        return false;
    }

    return true;
}

void VulkanRenderer::shutdown()
{
    initialized_ = false;
    swapchainOutOfDate_ = false;
    sceneObjects_.clear();
    meshRanges_.clear();

    if (device_ != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(device_);

        if (imageAvailable_ != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(device_, imageAvailable_, nullptr);
            imageAvailable_ = VK_NULL_HANDLE;
        }

        if (renderFinished_ != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(device_, renderFinished_, nullptr);
            renderFinished_ = VK_NULL_HANDLE;
        }

        if (commandPool_ != VK_NULL_HANDLE)
        {
            // Уничтожение пула освобождает его командные буферы.
            vkDestroyCommandPool(device_, commandPool_, nullptr);
            commandPool_ = VK_NULL_HANDLE;
        }

        commandBuffers_.clear();

        if (vertexBuffer_ != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(device_, vertexBuffer_, nullptr);
            vertexBuffer_ = VK_NULL_HANDLE;
        }

        if (vertexBufferMemory_ != VK_NULL_HANDLE)
        {
            vkFreeMemory(device_, vertexBufferMemory_, nullptr);
            vertexBufferMemory_ = VK_NULL_HANDLE;
        }

        if (graphicsPipeline_ != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(device_, graphicsPipeline_, nullptr);
            graphicsPipeline_ = VK_NULL_HANDLE;
        }

        if (pipelineLayout_ != VK_NULL_HANDLE)
        {
            vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
            pipelineLayout_ = VK_NULL_HANDLE;
        }

        for (VkFramebuffer framebuffer : framebuffers_)
        {
            if (framebuffer != VK_NULL_HANDLE)
            {
                vkDestroyFramebuffer(device_, framebuffer, nullptr);
            }
        }

        framebuffers_.clear();

        if (depthImageView_ != VK_NULL_HANDLE)
        {
            vkDestroyImageView(device_, depthImageView_, nullptr);
            depthImageView_ = VK_NULL_HANDLE;
        }

        if (depthImage_ != VK_NULL_HANDLE)
        {
            vkDestroyImage(device_, depthImage_, nullptr);
            depthImage_ = VK_NULL_HANDLE;
        }

        if (depthMemory_ != VK_NULL_HANDLE)
        {
            vkFreeMemory(device_, depthMemory_, nullptr);
            depthMemory_ = VK_NULL_HANDLE;
        }

        depthFormat_ = VK_FORMAT_UNDEFINED;

        for (VkImageView imageView : imageViews_)
        {
            if (imageView != VK_NULL_HANDLE)
            {
                vkDestroyImageView(device_, imageView, nullptr);
            }
        }

        imageViews_.clear();

        if (renderPass_ != VK_NULL_HANDLE)
        {
            vkDestroyRenderPass(device_, renderPass_, nullptr);
            renderPass_ = VK_NULL_HANDLE;
        }

        if (swapchain_ != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(device_, swapchain_, nullptr);
            swapchain_ = VK_NULL_HANDLE;
        }

        images_.clear();

        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }

    queue_ = VK_NULL_HANDLE;
    physicalDevice_ = VK_NULL_HANDLE;

    if (surface_ != VK_NULL_HANDLE &&
        instance_ != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }

    if (instance_ != VK_NULL_HANDLE)
    {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }

    swapchainFormat_ = VK_FORMAT_UNDEFINED;
    swapchainExtent_ = {};
    queueFamilyIndex_ = 0;
}
