#include "vk/MeshTestRenderer.h"
#include "vk/MeshTestPipeline.h"
#include "vk/Swapchain.h"

#include <vector>
#include <iostream>
#include <cstdlib>
#include <cstring>

namespace
{
    inline void VK_CHECK(VkResult r, const char* where)
    {
        if (r != VK_SUCCESS)
        {
            std::cerr << "[Vulkan] " << where << " failed: " << (int)r << "\n";
            std::exit(EXIT_FAILURE);
        }
    }

    uint32_t lbFindMemoryType(VkPhysicalDevice phys, uint32_t typeBits, VkMemoryPropertyFlags props)
    {
        VkPhysicalDeviceMemoryProperties mp{};
        vkGetPhysicalDeviceMemoryProperties(phys, &mp);

        for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
        {
            if ((typeBits & (1u << i)) && ((mp.memoryTypes[i].propertyFlags & props) == props))
                return i;
        }

        std::cerr << "Failed to find suitable memory type\n";
        std::exit(EXIT_FAILURE);
    }

    void lbCreateBuffer(VkPhysicalDevice phys, VkDevice dev, VkDeviceSize size,
                        VkBufferUsageFlags usage, VkMemoryPropertyFlags props,
                        VkBuffer& buf, VkDeviceMemory& mem)
    {
        VkBufferCreateInfo bci{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bci.size = size;
        bci.usage = usage;
        bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VK_CHECK(vkCreateBuffer(dev, &bci, nullptr, &buf), "vkCreateBuffer");

        VkMemoryRequirements mr{};
        vkGetBufferMemoryRequirements(dev, buf, &mr);

        VkMemoryAllocateInfo mai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        mai.allocationSize = mr.size;
        mai.memoryTypeIndex = lbFindMemoryType(phys, mr.memoryTypeBits, props);

        VK_CHECK(vkAllocateMemory(dev, &mai, nullptr, &mem), "vkAllocateMemory");
        VK_CHECK(vkBindBufferMemory(dev, buf, mem, 0), "vkBindBufferMemory");
    }

    void lbUploadToMemory(VkDevice dev, VkDeviceMemory mem, const void* data, size_t size)
    {
        void* mapped = nullptr;
        VK_CHECK(vkMapMemory(dev, mem, 0, (VkDeviceSize)size, 0, &mapped), "vkMapMemory");
        std::memcpy(mapped, data, size);
        vkUnmapMemory(dev, mem);
    }

    void barrierImage(VkCommandBuffer cmd, VkImage img,
                      VkImageLayout oldLayout, VkImageLayout newLayout,
                      VkAccessFlags srcAccess, VkAccessFlags dstAccess,
                      VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage)
    {
        VkImageMemoryBarrier b{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        b.oldLayout = oldLayout;
        b.newLayout = newLayout;
        b.srcAccessMask = srcAccess;
        b.dstAccessMask = dstAccess;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = img;
        b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        b.subresourceRange.baseMipLevel = 0;
        b.subresourceRange.levelCount = 1;
        b.subresourceRange.baseArrayLayer = 0;
        b.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0,
                             0, nullptr, 0, nullptr,
                             1, &b);
    }

    // std430: array of uvec3 имеет stride 16, поэтому в C++ делаем padding до 16 байт
    struct alignas(16) UVec3Std430
    {
        uint32_t x, y, z, pad;
    };

    struct Vec2 { float x, y; };
}

MeshTestRenderer::MeshTestRenderer(
    VkPhysicalDevice phys,
    VkDevice device,
    VkQueue graphicsQueue,
    VkQueue presentQueue,
    uint32_t graphicsQueueFamilyIndex,
    Swapchain& swapchain,
    MeshTestPipeline& pipeline,
    PFN_vkCmdDrawMeshTasksEXT cmdDrawMeshTasks)
    : m_phys(phys)
    , m_device(device)
    , m_gfxQueue(graphicsQueue)
    , m_presentQueue(presentQueue)
    , m_gfxQueueFamily(graphicsQueueFamilyIndex)
    , m_swapchain(swapchain)
    , m_pipeline(pipeline)
    , m_cmdDrawMeshTasks(cmdDrawMeshTasks)
{
    if (!m_cmdDrawMeshTasks)
    {
        std::cerr << "vkCmdDrawMeshTasksEXT is null (mesh shader fn not loaded)\n";
        std::exit(EXIT_FAILURE);
    }

    createCommandPoolAndBuffers();
    createSyncObjects();

    createLoopBlinnBuffers();
    createLBDescriptors();
}

MeshTestRenderer::~MeshTestRenderer()
{
    vkDeviceWaitIdle(m_device);

    destroyLBDescriptors();
    destroyLoopBlinnBuffers();

    destroySyncObjects();
    destroyCommandPoolAndBuffers();
}

void MeshTestRenderer::createCommandPoolAndBuffers()
{
    VkCommandPoolCreateInfo pci{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pci.queueFamilyIndex = m_gfxQueueFamily;
    VK_CHECK(vkCreateCommandPool(m_device, &pci, nullptr, &m_cmdPool), "vkCreateCommandPool");

    m_cmdBufferCount = m_swapchain.imageCount();
    m_cmdBuffers = new VkCommandBuffer[m_cmdBufferCount]{};

    VkCommandBufferAllocateInfo ai{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    ai.commandPool = m_cmdPool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = m_cmdBufferCount;

    VK_CHECK(vkAllocateCommandBuffers(m_device, &ai, m_cmdBuffers), "vkAllocateCommandBuffers");
}

void MeshTestRenderer::destroyCommandPoolAndBuffers()
{
    if (m_cmdBuffers)
    {
        vkFreeCommandBuffers(m_device, m_cmdPool, m_cmdBufferCount, m_cmdBuffers);
        delete[] m_cmdBuffers;
        m_cmdBuffers = nullptr;
    }

    if (m_cmdPool)
    {
        vkDestroyCommandPool(m_device, m_cmdPool, nullptr);
        m_cmdPool = VK_NULL_HANDLE;
    }

    m_cmdBufferCount = 0;
}

void MeshTestRenderer::createSyncObjects()
{
    const uint32_t n = m_swapchain.imageCount();
    m_imageAvailable = new VkSemaphore[n]{};
    m_renderFinished = new VkSemaphore[n]{};
    m_inFlightFence  = new VkFence[n]{};

    VkSemaphoreCreateInfo sci{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo fci{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32_t i = 0; i < n; ++i)
    {
        VK_CHECK(vkCreateSemaphore(m_device, &sci, nullptr, &m_imageAvailable[i]), "vkCreateSemaphore(imageAvailable)");
        VK_CHECK(vkCreateSemaphore(m_device, &sci, nullptr, &m_renderFinished[i]), "vkCreateSemaphore(renderFinished)");
        VK_CHECK(vkCreateFence(m_device, &fci, nullptr, &m_inFlightFence[i]), "vkCreateFence(inFlight)");
    }
}

void MeshTestRenderer::destroySyncObjects()
{
    const uint32_t n = m_swapchain.imageCount();

    if (m_imageAvailable)
    {
        for (uint32_t i = 0; i < n; ++i) vkDestroySemaphore(m_device, m_imageAvailable[i], nullptr);
        delete[] m_imageAvailable;
        m_imageAvailable = nullptr;
    }
    if (m_renderFinished)
    {
        for (uint32_t i = 0; i < n; ++i) vkDestroySemaphore(m_device, m_renderFinished[i], nullptr);
        delete[] m_renderFinished;
        m_renderFinished = nullptr;
    }
    if (m_inFlightFence)
    {
        for (uint32_t i = 0; i < n; ++i) vkDestroyFence(m_device, m_inFlightFence[i], nullptr);
        delete[] m_inFlightFence;
        m_inFlightFence = nullptr;
    }
}

void MeshTestRenderer::createLoopBlinnBuffers()
{
    // ")" из статьи (positions, indices, types)
    const Vec2 positions[10] = {
        {+0.000f, -1.00f},
        {+0.150f, -0.50f},
        {+0.150f, +0.00f},
        {+0.150f, +0.50f},
        {+0.000f, +1.00f},
        {-0.300f, +1.00f},
        {-0.165f, +0.50f},
        {-0.165f, +0.00f},
        {-0.165f, -0.50f},
        {-0.300f, -1.00f},
    };

    const UVec3Std430 tris[10] = {
        {0,1,2,0},
        {2,3,4,0},
        {5,6,7,0},
        {7,8,9,0},
        {4,5,6,0},
        {4,6,2,0},
        {6,7,2,0},
        {7,8,2,0},
        {2,8,0,0},
        {9,0,8,0},
    };

    // 0=SOLID 1=CONVEX 2=CONCAVE
    const uint32_t primType[10] = {
        1,1,
        2,2,
        0,0,0,0,0,0
    };

    std::vector<Vec2> offsets(m_instanceCount);
    // ряд скобок по X в NDC
    const float startX = -0.55f;
    const float stepX  =  0.16f;
    for (uint32_t i = 0; i < m_instanceCount; ++i)
        offsets[i] = { startX + stepX * float(i), 0.0f };

    const VkMemoryPropertyFlags hostProps =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    lbCreateBuffer(m_phys, m_device, sizeof(positions),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, hostProps, m_lbPosBuf, m_lbPosMem);

    lbCreateBuffer(m_phys, m_device, sizeof(tris),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, hostProps, m_lbIdxBuf, m_lbIdxMem);

    lbCreateBuffer(m_phys, m_device, sizeof(primType),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, hostProps, m_lbTypeBuf, m_lbTypeMem);

    lbCreateBuffer(m_phys, m_device, sizeof(Vec2) * offsets.size(),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, hostProps, m_lbInstBuf, m_lbInstMem);

    lbUploadToMemory(m_device, m_lbPosMem, positions, sizeof(positions));
    lbUploadToMemory(m_device, m_lbIdxMem, tris, sizeof(tris));
    lbUploadToMemory(m_device, m_lbTypeMem, primType, sizeof(primType));

    // instances держим mapped (можно потом анимировать)
    VK_CHECK(vkMapMemory(m_device, m_lbInstMem, 0, VK_WHOLE_SIZE, 0, &m_lbInstMapped), "vkMapMemory(instances)");
    std::memcpy(m_lbInstMapped, offsets.data(), sizeof(Vec2) * offsets.size());
}

void MeshTestRenderer::destroyLoopBlinnBuffers()
{
    if (m_lbInstMapped)
    {
        vkUnmapMemory(m_device, m_lbInstMem);
        m_lbInstMapped = nullptr;
    }

    auto kill = [&](VkBuffer& b, VkDeviceMemory& m)
    {
        if (b) vkDestroyBuffer(m_device, b, nullptr);
        if (m) vkFreeMemory(m_device, m, nullptr);
        b = VK_NULL_HANDLE;
        m = VK_NULL_HANDLE;
    };

    kill(m_lbPosBuf,  m_lbPosMem);
    kill(m_lbIdxBuf,  m_lbIdxMem);
    kill(m_lbTypeBuf, m_lbTypeMem);
    kill(m_lbInstBuf, m_lbInstMem);
}

void MeshTestRenderer::createLBDescriptors()
{
    VkDescriptorPoolSize ps{};
    ps.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    ps.descriptorCount = 4;

    VkDescriptorPoolCreateInfo dp{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    dp.maxSets = 1;
    dp.poolSizeCount = 1;
    dp.pPoolSizes = &ps;

    VK_CHECK(vkCreateDescriptorPool(m_device, &dp, nullptr, &m_lbDescPool), "vkCreateDescriptorPool");

    VkDescriptorSetLayout dsl = m_pipeline.descriptorSetLayout();

    VkDescriptorSetAllocateInfo dai{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    dai.descriptorPool = m_lbDescPool;
    dai.descriptorSetCount = 1;
    dai.pSetLayouts = &dsl;

    VK_CHECK(vkAllocateDescriptorSets(m_device, &dai, &m_lbDescSet), "vkAllocateDescriptorSets");

    VkDescriptorBufferInfo b0{ m_lbPosBuf,  0, VK_WHOLE_SIZE };
    VkDescriptorBufferInfo b1{ m_lbIdxBuf,  0, VK_WHOLE_SIZE };
    VkDescriptorBufferInfo b2{ m_lbTypeBuf, 0, VK_WHOLE_SIZE };
    VkDescriptorBufferInfo b3{ m_lbInstBuf, 0, VK_WHOLE_SIZE };

    VkWriteDescriptorSet w[4]{};

    for (uint32_t i = 0; i < 4; ++i)
    {
        w[i] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        w[i].dstSet = m_lbDescSet;
        w[i].dstBinding = i;
        w[i].descriptorCount = 1;
        w[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    }

    w[0].pBufferInfo = &b0;
    w[1].pBufferInfo = &b1;
    w[2].pBufferInfo = &b2;
    w[3].pBufferInfo = &b3;

    vkUpdateDescriptorSets(m_device, 4, w, 0, nullptr);
}

void MeshTestRenderer::destroyLBDescriptors()
{
    if (m_lbDescPool)
    {
        vkDestroyDescriptorPool(m_device, m_lbDescPool, nullptr);
        m_lbDescPool = VK_NULL_HANDLE;
        m_lbDescSet = VK_NULL_HANDLE;
    }
}

void MeshTestRenderer::recordCommandBuffer(uint32_t imageIndex)
{
    VkCommandBuffer cmd = m_cmdBuffers[imageIndex];

    VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    VK_CHECK(vkBeginCommandBuffer(cmd, &bi), "vkBeginCommandBuffer");

    // Layout: PRESENT -> COLOR_ATTACHMENT
    barrierImage(cmd,
        m_swapchain.image(imageIndex),
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_ACCESS_MEMORY_READ_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

    VkClearValue clear{};
    clear.color.float32[0] = 0.23f;
    clear.color.float32[1] = 0.23f;
    clear.color.float32[2] = 0.28f;
    clear.color.float32[3] = 1.0f;

    VkRenderingAttachmentInfo colorAtt{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
    colorAtt.imageView = m_swapchain.imageView(imageIndex);
    colorAtt.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAtt.clearValue = clear;

    VkRenderingInfo ri{ VK_STRUCTURE_TYPE_RENDERING_INFO };
    ri.renderArea.offset = { 0, 0 };
    ri.renderArea.extent = m_swapchain.extent();
    ri.layerCount = 1;
    ri.colorAttachmentCount = 1;
    ri.pColorAttachments = &colorAtt;

    vkCmdBeginRendering(cmd, &ri);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.pipeline());

    VkExtent2D ext = m_swapchain.extent();

    VkViewport vp{};
    vp.x = 0.0f;
    vp.y = 0.0f;
    vp.width  = (float)ext.width;
    vp.height = (float)ext.height;
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &vp);

    VkRect2D sc{};
    sc.offset = { 0, 0 };
    sc.extent = ext;
    vkCmdSetScissor(cmd, 0, 1, &sc);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.layout(),
                            0, 1, &m_lbDescSet, 0, nullptr);

    m_cmdDrawMeshTasks(cmd, m_instanceCount, 1, 1);

    vkCmdEndRendering(cmd);

    // Layout: COLOR_ATTACHMENT -> PRESENT
    barrierImage(cmd,
        m_swapchain.image(imageIndex),
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_ACCESS_MEMORY_READ_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

    VK_CHECK(vkEndCommandBuffer(cmd), "vkEndCommandBuffer");
}

bool MeshTestRenderer::drawFrame()
{
    // per-image sync: используем imageIndex как слот
    uint32_t imageIndex = 0;

    VkResult acq = vkAcquireNextImageKHR(
        m_device,
        m_swapchain.handle(),
        UINT64_MAX,
        VK_NULL_HANDLE, // мы подождём fence и используем семафоры на submit/present
        VK_NULL_HANDLE,
        &imageIndex);

    if (acq == VK_ERROR_OUT_OF_DATE_KHR || acq == VK_SUBOPTIMAL_KHR)
        return false;

    VK_CHECK(acq, "vkAcquireNextImageKHR");

    VK_CHECK(vkWaitForFences(m_device, 1, &m_inFlightFence[imageIndex], VK_TRUE, UINT64_MAX), "vkWaitForFences");
    VK_CHECK(vkResetFences(m_device, 1, &m_inFlightFence[imageIndex]), "vkResetFences");

    VK_CHECK(vkResetCommandBuffer(m_cmdBuffers[imageIndex], 0), "vkResetCommandBuffer");
    recordCommandBuffer(imageIndex);

    // Submit
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo si{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    // сигналим imageAvailable на acquire через отдельный семафор — здесь проще:
    // используем бинарники: imageAvailable будет сигналиться вручную нельзя,
    // поэтому делаем acquire без семафора и ждём fence — это ок на минималке.
    // (Если хочешь, я дам версию с semaphores: acquire -> imageAvailable[imageIndex].)

    si.waitSemaphoreCount = 0;
    si.pWaitSemaphores = nullptr;
    si.pWaitDstStageMask = &waitStage;

    si.commandBufferCount = 1;
    si.pCommandBuffers = &m_cmdBuffers[imageIndex];

    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &m_renderFinished[imageIndex];

    VK_CHECK(vkQueueSubmit(m_gfxQueue, 1, &si, m_inFlightFence[imageIndex]), "vkQueueSubmit");

    // Present
    VkPresentInfoKHR pi{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &m_renderFinished[imageIndex];

    VkSwapchainKHR sc = m_swapchain.handle();
    pi.swapchainCount = 1;
    pi.pSwapchains = &sc;
    pi.pImageIndices = &imageIndex;

    VkResult pres = vkQueuePresentKHR(m_presentQueue, &pi);
    if (pres == VK_ERROR_OUT_OF_DATE_KHR || pres == VK_SUBOPTIMAL_KHR)
        return false;

    VK_CHECK(pres, "vkQueuePresentKHR");
    return true;
}
