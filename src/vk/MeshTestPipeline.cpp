#include "vk/MeshTestPipeline.h"
#include "vk/VulkanUtils.h"

#include <fstream>
#include <vector>
#include <string>
#include <iostream>

static std::vector<uint32_t> read_spv_u32(const std::string& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
    {
        std::cerr << "Failed to open SPV: " << path << "\n";
        std::exit(EXIT_FAILURE);
    }

    const std::streamsize size = file.tellg();
    if (size <= 0 || (size % 4) != 0)
    {
        std::cerr << "Invalid SPV size: " << path << "\n";
        std::exit(EXIT_FAILURE);
    }

    std::vector<uint32_t> data((size_t)size / 4);
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(data.data()), size);
    return data;
}

static VkShaderModule create_shader_module(VkDevice device, const std::vector<uint32_t>& code)
{
    VkShaderModuleCreateInfo ci{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    ci.codeSize = code.size() * sizeof(uint32_t);
    ci.pCode = code.data();

    VkShaderModule mod = VK_NULL_HANDLE;
    vk_check(vkCreateShaderModule(device, &ci, nullptr, &mod), "vkCreateShaderModule");
    return mod;
}

MeshTestPipeline::MeshTestPipeline(VkDevice device, VkFormat colorFormat)
    : m_device(device), m_colorFormat(colorFormat)
{
    createLayouts();
    createPipeline();
}

MeshTestPipeline::~MeshTestPipeline()
{
    destroyAll();
}

void MeshTestPipeline::destroyPipeline()
{
    if (m_pipeline) vkDestroyPipeline(m_device, m_pipeline, nullptr);
    m_pipeline = VK_NULL_HANDLE;
}

void MeshTestPipeline::destroyAll()
{
    destroyPipeline();

    if (m_layout) vkDestroyPipelineLayout(m_device, m_layout, nullptr);
    m_layout = VK_NULL_HANDLE;

    if (m_setLayout) vkDestroyDescriptorSetLayout(m_device, m_setLayout, nullptr);
    m_setLayout = VK_NULL_HANDLE;
}

void MeshTestPipeline::recreate(VkFormat colorFormat)
{
    if (colorFormat == m_colorFormat) return;
    m_colorFormat = colorFormat;
    destroyPipeline();
    createPipeline();
}

void MeshTestPipeline::createLayouts()
{
    // binding 0: atlas sampler (frag)
    VkDescriptorSetLayoutBinding b0{};
    b0.binding = 0;
    b0.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    b0.descriptorCount = 1;
    b0.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    // binding 1: instances SSBO (mesh)
    VkDescriptorSetLayoutBinding b1{};
    b1.binding = 1;
    b1.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    b1.descriptorCount = 1;
    b1.stageFlags = VK_SHADER_STAGE_MESH_BIT_EXT;

    VkDescriptorSetLayoutBinding bindings[2] = { b0, b1 };

    VkDescriptorSetLayoutCreateInfo sl{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    sl.bindingCount = 2;
    sl.pBindings = bindings;

    vk_check(vkCreateDescriptorSetLayout(m_device, &sl, nullptr, &m_setLayout),
             "vkCreateDescriptorSetLayout");

    // push constants: vec4(params) = 16 bytes
    VkPushConstantRange pcr{};
    pcr.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pcr.offset = 0;
    pcr.size = 16;

    VkPipelineLayoutCreateInfo pl{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pl.setLayoutCount = 1;
    pl.pSetLayouts = &m_setLayout;
    pl.pushConstantRangeCount = 1;
    pl.pPushConstantRanges = &pcr;

    vk_check(vkCreatePipelineLayout(m_device, &pl, nullptr, &m_layout),
             "vkCreatePipelineLayout");
}

void MeshTestPipeline::createPipeline()
{
    const std::string base = std::string(APP_SHADER_DIR);
    const std::string meshPath = base + "/mesh_test.mesh.spv";
    const std::string fragPath = base + "/mesh_test.frag.spv";

    auto meshCode = read_spv_u32(meshPath);
    auto fragCode = read_spv_u32(fragPath);

    VkShaderModule meshMod = create_shader_module(m_device, meshCode);
    VkShaderModule fragMod = create_shader_module(m_device, fragCode);

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    stages[0].stage = VK_SHADER_STAGE_MESH_BIT_EXT;
    stages[0].module = meshMod;
    stages[0].pName = "main";

    stages[1] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragMod;
    stages[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vi{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

    VkPipelineInputAssemblyStateCreateInfo ia{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vp{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    vp.viewportCount = 1;
    vp.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rs{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState cba{};
    cba.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    cba.blendEnable = VK_TRUE;
    cba.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    cba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cba.colorBlendOp = VK_BLEND_OP_ADD;
    cba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    cba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cba.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo cb{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    cb.attachmentCount = 1;
    cb.pAttachments = &cba;

    VkDynamicState dynStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dyn{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates = dynStates;

    VkPipelineRenderingCreateInfo rendering{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    rendering.colorAttachmentCount = 1;
    rendering.pColorAttachmentFormats = &m_colorFormat;

    VkGraphicsPipelineCreateInfo gp{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    gp.pNext = &rendering;
    gp.stageCount = 2;
    gp.pStages = stages;
    gp.pVertexInputState = &vi;
    gp.pInputAssemblyState = &ia;
    gp.pViewportState = &vp;
    gp.pRasterizationState = &rs;
    gp.pMultisampleState = &ms;
    gp.pColorBlendState = &cb;
    gp.pDynamicState = &dyn;
    gp.layout = m_layout;
    gp.renderPass = VK_NULL_HANDLE;
    gp.subpass = 0;

    vk_check(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &gp, nullptr, &m_pipeline),
             "vkCreateGraphicsPipelines(msdf-text)");

    vkDestroyShaderModule(m_device, fragMod, nullptr);
    vkDestroyShaderModule(m_device, meshMod, nullptr);
}
