#pragma once

#include <vulkan/vulkan.h>
#include <array>
#include <vector>

class Swapchain;

class ClearRenderer
{
public:
    ClearRenderer(
        VkPhysicalDevice phys,
        VkDevice device,
        VkQueue graphicsQueue,
        VkQueue presentQueue,
        uint32_t graphicsFamily,
        Swapchain& swapchain);

    ~ClearRenderer();

    ClearRenderer(const ClearRenderer&) = delete;
    ClearRenderer& operator=(const ClearRenderer&) = delete;

    bool drawFrame(int fbWidth, int fbHeight);

private:
    void createCommandPoolAndBuffers();
    void createSyncObjects();

    void createPerImageSemaphores();
    void destroyPerImageSemaphores();

    void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex);

private:
    VkPhysicalDevice m_phys = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;

    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;
    uint32_t m_graphicsFamily = 0;

    Swapchain& m_swapchain;

    VkCommandPool m_cmdPool = VK_NULL_HANDLE;

    static constexpr uint32_t kFramesInFlight = 2;
    uint32_t m_frameIndex = 0;

    std::array<VkCommandBuffer, kFramesInFlight> m_cmds{};

    // per-frame
    std::array<VkSemaphore, kFramesInFlight> m_imageAvailable{};
    std::array<VkFence,     kFramesInFlight> m_inFlight{};

    // per-swapchain-image (ВАЖНО)
    std::vector<VkSemaphore> m_renderFinishedPerImage;
};
