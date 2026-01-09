#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>

class Swapchain;
class MeshTestPipeline;

class MeshTestRenderer
{
public:
    MeshTestRenderer(
        VkPhysicalDevice phys,
        VkDevice device,
        VkQueue graphicsQueue,
        VkQueue presentQueue,
        uint32_t graphicsQueueFamilyIndex,
        Swapchain& swapchain,
        MeshTestPipeline& pipeline,
        PFN_vkCmdDrawMeshTasksEXT cmdDrawMeshTasks);

    ~MeshTestRenderer();

    MeshTestRenderer(const MeshTestRenderer&) = delete;
    MeshTestRenderer& operator=(const MeshTestRenderer&) = delete;

    // Возвращает false если swapchain out-of-date/suboptimal (тогда снаружи пересоздай swapchain и renderer/pipeline)
    bool drawFrame();

private:
    void createCommandPoolAndBuffers();
    void destroyCommandPoolAndBuffers();

    void createSyncObjects();
    void destroySyncObjects();

    void recordCommandBuffer(uint32_t imageIndex);

    // Loop–Blinn (glyphlets) resources
    void createLoopBlinnBuffers();
    void destroyLoopBlinnBuffers();

    void createLBDescriptors();
    void destroyLBDescriptors();

private:
    VkPhysicalDevice m_phys = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;

    VkQueue m_gfxQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;
    uint32_t m_gfxQueueFamily = 0;

    Swapchain& m_swapchain;
    MeshTestPipeline& m_pipeline;

    PFN_vkCmdDrawMeshTasksEXT m_cmdDrawMeshTasks = nullptr;

    VkCommandPool m_cmdPool = VK_NULL_HANDLE;

    VkCommandBuffer* m_cmdBuffers = nullptr;
    uint32_t m_cmdBufferCount = 0;

    // Per-swapchain-image sync (чтобы не ловить semaphore reuse ошибки)
    VkSemaphore* m_imageAvailable = nullptr;
    VkSemaphore* m_renderFinished = nullptr;
    VkFence*     m_inFlightFence  = nullptr;

    // Loop–Blinn SSBOs
    VkBuffer m_lbPosBuf = VK_NULL_HANDLE;
    VkDeviceMemory m_lbPosMem = VK_NULL_HANDLE;

    VkBuffer m_lbIdxBuf = VK_NULL_HANDLE;
    VkDeviceMemory m_lbIdxMem = VK_NULL_HANDLE;

    VkBuffer m_lbTypeBuf = VK_NULL_HANDLE;
    VkDeviceMemory m_lbTypeMem = VK_NULL_HANDLE;

    VkBuffer m_lbInstBuf = VK_NULL_HANDLE;
    VkDeviceMemory m_lbInstMem = VK_NULL_HANDLE;
    void* m_lbInstMapped = nullptr;

    uint32_t m_instanceCount = 8;

    // Descriptor (Loop–Blinn)
    VkDescriptorPool m_lbDescPool = VK_NULL_HANDLE;
    VkDescriptorSet  m_lbDescSet  = VK_NULL_HANDLE;
};
