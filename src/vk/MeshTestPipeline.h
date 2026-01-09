#pragma once
#include <vulkan/vulkan.h>

class MeshTestPipeline
{
public:
    MeshTestPipeline(VkDevice device, VkFormat colorFormat);
    ~MeshTestPipeline();

    MeshTestPipeline(const MeshTestPipeline&) = delete;
    MeshTestPipeline& operator=(const MeshTestPipeline&) = delete;

    void recreate(VkFormat colorFormat);

    VkPipeline pipeline() const { return m_pipeline; }
    VkPipelineLayout layout() const { return m_layout; }
    VkDescriptorSetLayout descriptorSetLayout() const { return m_setLayout; }

private:
    void createLayouts();
    void createPipeline();
    void destroyPipeline();
    void destroyAll();

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkFormat m_colorFormat = VK_FORMAT_UNDEFINED;

    VkDescriptorSetLayout m_setLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_layout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
};
