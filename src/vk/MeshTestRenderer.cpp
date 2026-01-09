#include "vk/MeshTestRenderer.h"
#include "vk/Swapchain.h"
#include "vk/MeshTestPipeline.h"
#include "vk/VulkanUtils.h"
#include "vk/MsdfFont.h"

#include <iostream>
#include <fstream>
#include <cstring>
#include <algorithm>

struct GlyphInstanceCPU
{
    float posMin[2];
    float posMax[2];
    float uvMin[2];
    float uvMax[2];
};
static_assert(sizeof(GlyphInstanceCPU) == 32);

static bool readFileBytes(const std::string& path, std::vector<uint8_t>& out)
{
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return false;
    std::streamsize sz = f.tellg();
    if (sz <= 0) return false;
    out.resize((size_t)sz);
    f.seekg(0, std::ios::beg);
    f.read(reinterpret_cast<char*>(out.data()), sz);
    return true;
}

// --- Vulkan helpers ---
static uint32_t find_memory_type(VkPhysicalDevice phys, uint32_t typeBits, VkMemoryPropertyFlags props)
{
    VkPhysicalDeviceMemoryProperties mp{};
    vkGetPhysicalDeviceMemoryProperties(phys, &mp);

    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
        if ((typeBits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & props) == props)
            return i;

    std::cerr << "Failed to find suitable memory type.\n";
    std::exit(EXIT_FAILURE);
}

static void create_buffer(
    VkPhysicalDevice phys,
    VkDevice device,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags props,
    VkBuffer& outBuf,
    VkDeviceMemory& outMem)
{
    VkBufferCreateInfo bci{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bci.size = size;
    bci.usage = usage;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vk_check(vkCreateBuffer(device, &bci, nullptr, &outBuf), "vkCreateBuffer");

    VkMemoryRequirements mr{};
    vkGetBufferMemoryRequirements(device, outBuf, &mr);

    VkMemoryAllocateInfo mai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    mai.allocationSize = mr.size;
    mai.memoryTypeIndex = find_memory_type(phys, mr.memoryTypeBits, props);

    vk_check(vkAllocateMemory(device, &mai, nullptr, &outMem), "vkAllocateMemory(buffer)");
    vk_check(vkBindBufferMemory(device, outBuf, outMem, 0), "vkBindBufferMemory");
}

static void create_image(
    VkPhysicalDevice phys,
    VkDevice device,
    uint32_t width,
    uint32_t height,
    VkFormat format,
    VkImageUsageFlags usage,
    VkImage& outImg,
    VkDeviceMemory& outMem)
{
    VkImageCreateInfo ici{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = format;
    ici.extent = { width, height, 1 };
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = usage;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vk_check(vkCreateImage(device, &ici, nullptr, &outImg), "vkCreateImage");

    VkMemoryRequirements mr{};
    vkGetImageMemoryRequirements(device, outImg, &mr);

    VkMemoryAllocateInfo mai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    mai.allocationSize = mr.size;
    mai.memoryTypeIndex = find_memory_type(phys, mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vk_check(vkAllocateMemory(device, &mai, nullptr, &outMem), "vkAllocateMemory(image)");
    vk_check(vkBindImageMemory(device, outImg, outMem, 0), "vkBindImageMemory");
}

static void cmd_image_barrier(
    VkCommandBuffer cmd,
    VkImage img,
    VkImageLayout oldLayout,
    VkImageLayout newLayout,
    VkAccessFlags srcAccess,
    VkAccessFlags dstAccess,
    VkPipelineStageFlags srcStage,
    VkPipelineStageFlags dstStage)
{
    VkImageMemoryBarrier b{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    b.oldLayout = oldLayout;
    b.newLayout = newLayout;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = img;
    b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    b.subresourceRange.baseMipLevel = 0;
    b.subresourceRange.levelCount = 1;
    b.subresourceRange.baseArrayLayer = 0;
    b.subresourceRange.layerCount = 1;
    b.srcAccessMask = srcAccess;
    b.dstAccessMask = dstAccess;

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &b);
}

static float px_to_ndc_x(float x, float w) { return (x / w) * 2.0f - 1.0f; }
static float px_to_ndc_y(float y, float h) { return (y / h) * 2.0f - 1.0f; }

// --- State ---
static constexpr uint32_t kMaxGlyphs = 512;

static MsdfFont g_font;

// ---------------------------
MeshTestRenderer::MeshTestRenderer(
    VkPhysicalDevice phys,
    VkDevice device,
    VkQueue graphicsQueue,
    VkQueue presentQueue,
    uint32_t graphicsFamily,
    Swapchain& swapchain,
    MeshTestPipeline& pipeline,
    PFN_vkCmdDrawMeshTasksEXT drawMeshTasks)
    : m_phys(phys)
    , m_device(device)
    , m_graphicsQueue(graphicsQueue)
    , m_presentQueue(presentQueue)
    , m_graphicsFamily(graphicsFamily)
    , m_swapchain(swapchain)
    , m_pipeline(pipeline)
    , m_drawMeshTasks(drawMeshTasks)
{
    if (!m_drawMeshTasks)
    {
        std::cerr << "vkCmdDrawMeshTasksEXT is null.\n";
        std::exit(EXIT_FAILURE);
    }

    createCommandPoolAndBuffers();
    createSyncObjects();
    createPerImageSemaphores();
    createMsdfResources();
}

MeshTestRenderer::~MeshTestRenderer()
{
    vkDeviceWaitIdle(m_device);

    destroyMsdfResources();
    destroyPerImageSemaphores();

    for (uint32_t i = 0; i < kFramesInFlight; ++i)
    {
        if (m_imageAvailable[i]) vkDestroySemaphore(m_device, m_imageAvailable[i], nullptr);
        if (m_inFlight[i])       vkDestroyFence(m_device, m_inFlight[i], nullptr);
    }

    if (m_cmdPool) vkDestroyCommandPool(m_device, m_cmdPool, nullptr);
}

void MeshTestRenderer::createCommandPoolAndBuffers()
{
    VkCommandPoolCreateInfo pci{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    pci.queueFamilyIndex = m_graphicsFamily;
    pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    vk_check(vkCreateCommandPool(m_device, &pci, nullptr, &m_cmdPool), "vkCreateCommandPool");

    VkCommandBufferAllocateInfo ai{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    ai.commandPool = m_cmdPool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = kFramesInFlight;

    vk_check(vkAllocateCommandBuffers(m_device, &ai, m_cmds.data()), "vkAllocateCommandBuffers");
}

void MeshTestRenderer::createSyncObjects()
{
    VkSemaphoreCreateInfo sci{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo fci{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32_t i = 0; i < kFramesInFlight; ++i)
    {
        vk_check(vkCreateSemaphore(m_device, &sci, nullptr, &m_imageAvailable[i]), "vkCreateSemaphore(imageAvailable)");
        vk_check(vkCreateFence(m_device, &fci, nullptr, &m_inFlight[i]), "vkCreateFence(inFlight)");
    }
}

void MeshTestRenderer::destroyPerImageSemaphores()
{
    for (auto s : m_renderFinishedPerImage)
        vkDestroySemaphore(m_device, s, nullptr);
    m_renderFinishedPerImage.clear();
}

void MeshTestRenderer::createPerImageSemaphores()
{
    destroyPerImageSemaphores();

    VkSemaphoreCreateInfo sci{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    m_renderFinishedPerImage.resize(m_swapchain.images().size(), VK_NULL_HANDLE);

    for (size_t i = 0; i < m_renderFinishedPerImage.size(); ++i)
        vk_check(vkCreateSemaphore(m_device, &sci, nullptr, &m_renderFinishedPerImage[i]),
                 "vkCreateSemaphore(renderFinishedPerImage)");
}

void MeshTestRenderer::destroyMsdfResources()
{
    if (m_instancesMapped)
    {
        vkUnmapMemory(m_device, m_instancesMem);
        m_instancesMapped = nullptr;
    }

    if (m_descPool) vkDestroyDescriptorPool(m_device, m_descPool, nullptr);
    m_descPool = VK_NULL_HANDLE;
    m_descSet = VK_NULL_HANDLE;

    if (m_instancesBuf) vkDestroyBuffer(m_device, m_instancesBuf, nullptr);
    if (m_instancesMem) vkFreeMemory(m_device, m_instancesMem, nullptr);
    m_instancesBuf = VK_NULL_HANDLE;
    m_instancesMem = VK_NULL_HANDLE;

    if (m_atlasSampler) vkDestroySampler(m_device, m_atlasSampler, nullptr);
    if (m_atlasView) vkDestroyImageView(m_device, m_atlasView, nullptr);
    if (m_atlasImg) vkDestroyImage(m_device, m_atlasImg, nullptr);
    if (m_atlasMem) vkFreeMemory(m_device, m_atlasMem, nullptr);

    m_atlasSampler = VK_NULL_HANDLE;
    m_atlasView = VK_NULL_HANDLE;
    m_atlasImg = VK_NULL_HANDLE;
    m_atlasMem = VK_NULL_HANDLE;
}

void MeshTestRenderer::createMsdfResources()
{
    // 1) Load font.json
    const std::string base = std::string(APP_ASSETS_DIR);
    const std::string jsonPath = base + "/font.json";
    const std::string rgbaPath = base + "/font.rgba";

    if (!g_font.loadFromJson(jsonPath))
    {
        std::cerr << "Failed to load font json: " << jsonPath << "\n";
        std::exit(EXIT_FAILURE);
    }

    m_pxRange = g_font.pxRange();

    // 2) Load RGBA
    std::vector<uint8_t> rgba;
    if (!readFileBytes(rgbaPath, rgba))
    {
        std::cerr << "Failed to read atlas rgba: " << rgbaPath << "\n";
        std::exit(EXIT_FAILURE);
    }

    const int w = g_font.atlasW();
    const int h = g_font.atlasH();
    const size_t expected = (size_t)w * (size_t)h * 4u;

    // У тебя был кейс +12 байт — поддержим
    const uint8_t* pixelData = rgba.data();
    size_t pixelBytes = rgba.size();
    if (pixelBytes == expected + 12)
    {
        pixelData += 12;
        pixelBytes = expected;
    }
    else if (pixelBytes != expected)
    {
        std::cerr << "RGBA size mismatch: got " << rgba.size()
                  << ", expected " << expected
                  << " (" << w << "x" << h << ")\n";
        std::exit(EXIT_FAILURE);
    }

    // 3) Upload RGBA -> GPU image
    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory stagingMem = VK_NULL_HANDLE;

    create_buffer(m_phys, m_device, (VkDeviceSize)pixelBytes,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        staging, stagingMem);

    void* mapped = nullptr;
    vk_check(vkMapMemory(m_device, stagingMem, 0, (VkDeviceSize)pixelBytes, 0, &mapped), "vkMapMemory(staging)");
    std::memcpy(mapped, pixelData, pixelBytes);
    vkUnmapMemory(m_device, stagingMem);

    create_image(m_phys, m_device, (uint32_t)w, (uint32_t)h, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        m_atlasImg, m_atlasMem);

    VkCommandBufferAllocateInfo cai{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    cai.commandPool = m_cmdPool;
    cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cai.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    vk_check(vkAllocateCommandBuffers(m_device, &cai, &cmd), "vkAllocateCommandBuffers(upload)");

    VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vk_check(vkBeginCommandBuffer(cmd, &bi), "vkBeginCommandBuffer(upload)");

    cmd_image_barrier(cmd, m_atlasImg,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        0, VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = { (uint32_t)w, (uint32_t)h, 1 };

    vkCmdCopyBufferToImage(cmd, staging, m_atlasImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    cmd_image_barrier(cmd, m_atlasImg,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    vk_check(vkEndCommandBuffer(cmd), "vkEndCommandBuffer(upload)");

    VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;

    vk_check(vkQueueSubmit(m_graphicsQueue, 1, &submit, VK_NULL_HANDLE), "vkQueueSubmit(upload)");
    vk_check(vkQueueWaitIdle(m_graphicsQueue), "vkQueueWaitIdle(upload)");

    vkFreeCommandBuffers(m_device, m_cmdPool, 1, &cmd);

    vkDestroyBuffer(m_device, staging, nullptr);
    vkFreeMemory(m_device, stagingMem, nullptr);

    VkImageViewCreateInfo vci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    vci.image = m_atlasImg;
    vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vci.format = VK_FORMAT_R8G8B8A8_UNORM;
    vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vci.subresourceRange.levelCount = 1;
    vci.subresourceRange.layerCount = 1;
    vk_check(vkCreateImageView(m_device, &vci, nullptr, &m_atlasView), "vkCreateImageView(atlas)");

    VkSamplerCreateInfo sci{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    sci.magFilter = VK_FILTER_LINEAR;
    sci.minFilter = VK_FILTER_LINEAR;
    sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.maxLod = 0.0f;
    vk_check(vkCreateSampler(m_device, &sci, nullptr, &m_atlasSampler), "vkCreateSampler(atlas)");

    // 4) Instances SSBO (persistent map)
    create_buffer(m_phys, m_device,
        sizeof(GlyphInstanceCPU) * kMaxGlyphs,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        m_instancesBuf, m_instancesMem);

    vk_check(vkMapMemory(m_device, m_instancesMem, 0, VK_WHOLE_SIZE, 0, &m_instancesMapped),
             "vkMapMemory(instances)");

    // 5) Descriptor pool + set
    VkDescriptorPoolSize poolSizes[2]{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = 1;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[1].descriptorCount = 1;

    VkDescriptorPoolCreateInfo dp{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    dp.maxSets = 1;
    dp.poolSizeCount = 2;
    dp.pPoolSizes = poolSizes;

    vk_check(vkCreateDescriptorPool(m_device, &dp, nullptr, &m_descPool), "vkCreateDescriptorPool");

    VkDescriptorSetLayout dsl = m_pipeline.descriptorSetLayout();
    VkDescriptorSetAllocateInfo dai{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    dai.descriptorPool = m_descPool;
    dai.descriptorSetCount = 1;
    dai.pSetLayouts = &dsl;
    vk_check(vkAllocateDescriptorSets(m_device, &dai, &m_descSet), "vkAllocateDescriptorSets");

    VkDescriptorImageInfo ii{};
    ii.sampler = m_atlasSampler;
    ii.imageView = m_atlasView;
    ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorBufferInfo bi2{};
    bi2.buffer = m_instancesBuf;
    bi2.offset = 0;
    bi2.range = sizeof(GlyphInstanceCPU) * kMaxGlyphs;

    VkWriteDescriptorSet wds[2]{};
    wds[0] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    wds[0].dstSet = m_descSet;
    wds[0].dstBinding = 0;
    wds[0].descriptorCount = 1;
    wds[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    wds[0].pImageInfo = &ii;

    wds[1] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    wds[1].dstSet = m_descSet;
    wds[1].dstBinding = 1;
    wds[1].descriptorCount = 1;
    wds[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    wds[1].pBufferInfo = &bi2;

    vkUpdateDescriptorSets(m_device, 2, wds, 0, nullptr);

    std::cout << "MSDF ready. atlas=" << w << "x" << h << ", pxRange=" << m_pxRange << "\n";
}

void MeshTestRenderer::updateInstances(uint32_t screenW, uint32_t screenH)
{
    GlyphInstanceCPU* dst = reinterpret_cast<GlyphInstanceCPU*>(m_instancesMapped);
    m_glyphCount = 0;

    const auto& met = g_font.metrics();
    const float em = (met.emSize > 0.0f) ? met.emSize : 48.0f;
    const float scale = m_fontPx / em;

    float penX = m_startX;
    float baseY = m_baselineY;

    auto push_glyph = [&](const MsdfGlyph& g)
    {
        if (m_glyphCount >= kMaxGlyphs) return;

        if (!g.hasPlane || !g.hasAtlas)
            return;

        // screen coords (y down)
        float x0 = penX + g.plane.left  * scale;
        float x1 = penX + g.plane.right * scale;

        float yTop    = baseY - g.plane.top    * scale;
        float yBottom = baseY - g.plane.bottom * scale;

        // NDC
        float ndcL = px_to_ndc_x(x0, (float)screenW);
        float ndcR = px_to_ndc_x(x1, (float)screenW);
        float ndcT = px_to_ndc_y(yTop, (float)screenH);
        float ndcB = px_to_ndc_y(yBottom, (float)screenH);

        // atlas uv (v=0 top). msdf-atlas-gen atlasBounds обычно в px, y от низа -> надо флипнуть.
        float u0 = g.atlas.left  / (float)g_font.atlasW();
        float u1 = g.atlas.right / (float)g_font.atlasW();

        float vTop, vBottom;
        if (g_font.atlasYBottom())
        {
            vTop    = 1.0f - (g.atlas.top    / (float)g_font.atlasH());
            vBottom = 1.0f - (g.atlas.bottom / (float)g_font.atlasH());
        }
        else
        {
            vTop    = g.atlas.top    / (float)g_font.atlasH();
            vBottom = g.atlas.bottom / (float)g_font.atlasH();
        }


        GlyphInstanceCPU inst{};
        inst.posMin[0] = ndcL;
        inst.posMin[1] = ndcB;
        inst.posMax[0] = ndcR;
        inst.posMax[1] = ndcT;

        inst.uvMin[0] = u0;  inst.uvMin[1] = vTop;
        inst.uvMax[0] = u1;  inst.uvMax[1] = vBottom;

        dst[m_glyphCount++] = inst;
    };

    // ASCII (для старта). Дальше можно заменить на UTF-8 декодер.
    for (unsigned char ch : m_text)
    {
        if (ch == '\n')
        {
            penX = m_startX;
            float lh = (met.lineHeight != 0.0f) ? met.lineHeight : em;
            baseY += lh * scale;
            continue;
        }

        const MsdfGlyph* g = g_font.find((uint32_t)ch);
        if (!g)
            continue;

        push_glyph(*g);
        penX += g->advance * scale;
    }
}

void MeshTestRenderer::recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex)
{
    VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vk_check(vkBeginCommandBuffer(cmd, &bi), "vkBeginCommandBuffer");

    VkImage img = m_swapchain.images()[imageIndex];
    VkImageView view = m_swapchain.imageViews()[imageIndex];
    const VkExtent2D ext = m_swapchain.extent();

    const VkImageLayout old = m_swapchain.layoutOf(imageIndex);

    VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    if (old == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
        srcStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

    cmd_image_barrier(
        cmd,
        img,
        old,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        0,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        srcStage,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    );

    VkClearValue clear{};
    clear.color.float32[0] = 0.05f;
    clear.color.float32[1] = 0.05f;
    clear.color.float32[2] = 0.08f;
    clear.color.float32[3] = 1.0f;

    VkRenderingAttachmentInfo colorAtt{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
    colorAtt.imageView = view;
    colorAtt.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAtt.clearValue = clear;

    VkRenderingInfo ri{ VK_STRUCTURE_TYPE_RENDERING_INFO };
    ri.renderArea.offset = { 0, 0 };
    ri.renderArea.extent = ext;
    ri.layerCount = 1;
    ri.colorAttachmentCount = 1;
    ri.pColorAttachments = &colorAtt;

    vkCmdBeginRendering(cmd, &ri);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.pipeline());

    VkViewport vp{};
    vp.x = 0.0f;
    vp.y = 0.0f;
    vp.width = (float)ext.width;
    vp.height = (float)ext.height;
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &vp);

    VkRect2D sc{};
    sc.offset = { 0, 0 };
    sc.extent = ext;
    vkCmdSetScissor(cmd, 0, 1, &sc);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.layout(),
                            0, 1, &m_descSet, 0, nullptr);

    struct Push { float params[4]; } pc{ { m_pxRange, m_debugAtlas ? 1.f : 0.f, 0.f, 0.f } };
    vkCmdPushConstants(cmd, m_pipeline.layout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Push), &pc);

    if (m_glyphCount > 0)
        m_drawMeshTasks(cmd, m_glyphCount, 1, 1);

    vkCmdEndRendering(cmd);

    cmd_image_barrier(
        cmd,
        img,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        0,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
    );

    vk_check(vkEndCommandBuffer(cmd), "vkEndCommandBuffer");
}

bool MeshTestRenderer::drawFrame(int fbWidth, int fbHeight)
{
    const uint32_t fi = m_frameIndex;
    m_frameIndex = (m_frameIndex + 1) % kFramesInFlight;

    vk_check(vkWaitForFences(m_device, 1, &m_inFlight[fi], VK_TRUE, UINT64_MAX), "vkWaitForFences");
    vk_check(vkResetFences(m_device, 1, &m_inFlight[fi]), "vkResetFences");

    uint32_t imageIndex = 0;
    VkResult acq = vkAcquireNextImageKHR(
        m_device,
        m_swapchain.handle(),
        UINT64_MAX,
        m_imageAvailable[fi],
        VK_NULL_HANDLE,
        &imageIndex
    );

    if (acq == VK_ERROR_OUT_OF_DATE_KHR)
    {
        vkDeviceWaitIdle(m_device);
        m_swapchain.recreate(fbWidth, fbHeight);
        createPerImageSemaphores();
        m_pipeline.recreate(m_swapchain.format());
        return true;
    }
    vk_check(acq, "vkAcquireNextImageKHR");

    // обновляем инстансы под текущий размер окна
    updateInstances(m_swapchain.extent().width, m_swapchain.extent().height);

    VkSemaphore renderFinished = m_renderFinishedPerImage[imageIndex];

    vk_check(vkResetCommandBuffer(m_cmds[fi], 0), "vkResetCommandBuffer");
    recordCommandBuffer(m_cmds[fi], imageIndex);

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &m_imageAvailable[fi];
    submit.pWaitDstStageMask = &waitStage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &m_cmds[fi];
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &renderFinished;

    vk_check(vkQueueSubmit(m_graphicsQueue, 1, &submit, m_inFlight[fi]), "vkQueueSubmit");

    VkPresentInfoKHR present{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &renderFinished;
    VkSwapchainKHR sc = m_swapchain.handle();
    present.swapchainCount = 1;
    present.pSwapchains = &sc;
    present.pImageIndices = &imageIndex;

    VkResult pres = vkQueuePresentKHR(m_presentQueue, &present);
    if (pres == VK_ERROR_OUT_OF_DATE_KHR || pres == VK_SUBOPTIMAL_KHR)
    {
        vkDeviceWaitIdle(m_device);
        m_swapchain.recreate(fbWidth, fbHeight);
        createPerImageSemaphores();
        m_pipeline.recreate(m_swapchain.format());
        return true;
    }
    vk_check(pres, "vkQueuePresentKHR");

    m_swapchain.setLayout(imageIndex, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    return false;
}
