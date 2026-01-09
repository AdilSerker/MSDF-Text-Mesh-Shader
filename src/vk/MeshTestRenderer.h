#pragma once

#include <vulkan/vulkan.h>
#include <array>
#include <vector>
#include <string>
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
        uint32_t graphicsFamily,
        Swapchain& swapchain,
        MeshTestPipeline& pipeline,
        PFN_vkCmdDrawMeshTasksEXT drawMeshTasks);

    ~MeshTestRenderer();

    bool drawFrame(int fbWidth, int fbHeight);

private:
    void createCommandPoolAndBuffers();
    void createSyncObjects();
    void createPerImageSemaphores();
    void destroyPerImageSemaphores();

    void createMsdfResources();
    void destroyMsdfResources();

    void updateInstances(uint32_t screenW, uint32_t screenH); // обновляем SSBO
    void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex);

private:
    VkPhysicalDevice m_phys = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;

    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;
    uint32_t m_graphicsFamily = 0;

    Swapchain& m_swapchain;
    MeshTestPipeline& m_pipeline;
    PFN_vkCmdDrawMeshTasksEXT m_drawMeshTasks = nullptr;

    VkCommandPool m_cmdPool = VK_NULL_HANDLE;

    static constexpr uint32_t kFramesInFlight = 2;
    uint32_t m_frameIndex = 0;

    std::array<VkCommandBuffer, kFramesInFlight> m_cmds{};
    std::array<VkSemaphore,     kFramesInFlight> m_imageAvailable{};
    std::array<VkFence,         kFramesInFlight> m_inFlight{};

    std::vector<VkSemaphore> m_renderFinishedPerImage;

    // --- MSDF atlas texture ---
    VkImage m_atlasImg = VK_NULL_HANDLE;
    VkDeviceMemory m_atlasMem = VK_NULL_HANDLE;
    VkImageView m_atlasView = VK_NULL_HANDLE;
    VkSampler m_atlasSampler = VK_NULL_HANDLE;

    // --- Instances SSBO ---
    VkBuffer m_instancesBuf = VK_NULL_HANDLE;
    VkDeviceMemory m_instancesMem = VK_NULL_HANDLE;
    void* m_instancesMapped = nullptr;
    uint32_t m_glyphCount = 0;

    // --- Descriptor ---
    VkDescriptorPool m_descPool = VK_NULL_HANDLE;
    VkDescriptorSet  m_descSet = VK_NULL_HANDLE;

    // --- Text params ---
    float m_pxRange = 4.0f;
    bool  m_debugAtlas = false;
    bool  m_flipAtlasV = true;

    std::string m_text = "Nahuy tak zhit'!";
    float m_fontPx = 140.0f;     // высота шрифта на экране (примерно)
    float m_startX = 60.0f;      // px
    float m_baselineY = 180.0f;  // px от верха окна
};
