#pragma once

#include "../scene/TestScene.h"
#include "../world/ChunkGrid.h"
#include "../world/StonePlacement.h"

#define VK_USE_PLATFORM_ANDROID_KHR
#include <vulkan/vulkan.h>

#include <android/native_window.h>
#include <android/asset_manager.h>

#include <cstdint>
#include <string>
#include <vector>

class VulkanRenderer
{
public:
    VulkanRenderer() = default;
    ~VulkanRenderer();

    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;

    // Создаёт Vulkan-ресурсы для текущего окна.
    bool initialize(
        ANativeWindow* window,
        AAssetManager* assetManager,
        const char* scenePath = "scenes/physics.atlasscene"
    );

    // Выводит один кадр.
    bool drawFrame();
    bool updateRotation(
        float angleY,
        float angleX,
        float cameraDistance
    );

    // Освобождает все Vulkan-ресурсы.
    void shutdown();

    const std::vector<atlas::SceneObject>& sceneObjects() const { return sceneObjects_; }
    bool updateTerrain(atlas::world::ChunkCoord center);
    bool setObjectY(size_t index, float y);
    bool setObjectPosition(size_t index, float x, float y, float z);
    bool isInitialized() const;
    bool isSwapchainOutOfDate() const;

private:
    bool createInstance();
    bool createSurface(ANativeWindow* window);
    bool selectPhysicalDevice();
    bool createDevice();
    bool createSwapchain();
    bool createDepthResources();
    bool createRenderPass();
    bool createTextureDescriptorLayout();
    bool createTextureImage(VkImage& image, VkDeviceMemory& memory, uint32_t width, uint32_t height);
    bool uploadTexture(VkImage image, uint32_t width, uint32_t height, const std::vector<uint8_t>& pixels);
    bool createTerrainTextures(AAssetManager* assetManager);
    bool createTextureDescriptors();
    bool createGraphicsPipeline(AAssetManager* assetManager);

    bool loadShaderModule(
        AAssetManager* assetManager,
        const char* path,
        VkShaderModule* shaderModule
    );
    bool createImageViews();
    bool createFramebuffers();
    bool createVertexBuffer(AAssetManager* assetManager);
    bool createCommandResources();
    bool recordCommandBuffers(
        float angleY,
        float angleX,
        float cameraDistance
    );
    bool createSyncObjects();

    struct MeshRange
    {
        std::string name;
        uint32_t firstVertex = 0;
        uint32_t vertexCount = 0;
    };

    atlas::world::ChunkCoord terrainCenter_{};
    bool terrainBuffer_ = false;
    std::vector<atlas::world::StonePlacement> stones_;
    std::vector<MeshRange> meshRanges_;
    std::vector<atlas::SceneObject> sceneObjects_;

    VkInstance instance_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;

    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue queue_ = VK_NULL_HANDLE;

    uint32_t queueFamilyIndex_ = 0;

    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchainFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent_{};
    VkSurfaceTransformFlagBitsKHR surfaceTransform_ =
        VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;

    std::vector<VkImage> images_;
    std::vector<VkImageView> imageViews_;
    std::vector<VkFramebuffer> framebuffers_;
    std::vector<VkCommandBuffer> commandBuffers_;

    VkFormat depthFormat_ = VK_FORMAT_UNDEFINED;
    VkImage depthImage_ = VK_NULL_HANDLE;
    VkDeviceMemory depthMemory_ = VK_NULL_HANDLE;

    VkImage textureImage_ = VK_NULL_HANDLE;
    VkDeviceMemory textureMemory_ = VK_NULL_HANDLE;
    VkImageView textureImageView_ = VK_NULL_HANDLE;
    VkSampler textureSampler_ = VK_NULL_HANDLE;
    VkImage terrainImages_[3] = {};
    VkDeviceMemory terrainMemories_[3] = {};
    VkImageView terrainViews_[3] = {};
    VkDescriptorPool textureDescriptorPool_ = VK_NULL_HANDLE;
    VkDescriptorSet textureDescriptorSet_ = VK_NULL_HANDLE;
    VkImageView depthImageView_ = VK_NULL_HANDLE;

    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout textureDescriptorLayout_ = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline_ = VK_NULL_HANDLE;
    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory_ = VK_NULL_HANDLE;
    VkCommandPool commandPool_ = VK_NULL_HANDLE;

    VkSemaphore imageAvailable_ = VK_NULL_HANDLE;
    VkSemaphore renderFinished_ = VK_NULL_HANDLE;

    bool initialized_ = false;
    bool swapchainOutOfDate_ = false;
};
