#include "vk/ClearRenderer.h"
#include "vk/Swapchain.h"
#include "vk/VulkanUtils.h"

#include <iostream>

ClearRenderer::ClearRenderer(
    VkPhysicalDevice phys,
    VkDevice device,
    VkQueue graphicsQueue,
    VkQueue presentQueue,
    uint32_t graphicsFamily,
    Swapchain& swapchain)
    : m_phys(phys)
    , m_device(device)
    , m_graphicsQueue(graphicsQueue)
    , m_presentQueue(presentQueue)
    , m_graphicsFamily(graphicsFamily)
    , m_swapchain(swapchain)
{
    createCommandPoolAndBuffers();
    createSyncObjects();
    createPerImageSemaphores();
}

ClearRenderer::~ClearRenderer()
{
    vkDeviceWaitIdle(m_device);

    destroyPerImageSemaphores();

    for (uint32_t i = 0; i < kFramesInFlight; ++i)
    {
        if (m_imageAvailable[i]) vkDestroySemaphore(m_device, m_imageAvailable[i], nullptr);
        if (m_inFlight[i])       vkDestroyFence(m_device, m_inFlight[i], nullptr);
    }

    if (m_cmdPool) vkDestroyCommandPool(m_device, m_cmdPool, nullptr);
}

void ClearRenderer::createCommandPoolAndBuffers()
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

void ClearRenderer::createSyncObjects()
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

void ClearRenderer::destroyPerImageSemaphores()
{
    for (auto s : m_renderFinishedPerImage)
        vkDestroySemaphore(m_device, s, nullptr);
    m_renderFinishedPerImage.clear();
}

void ClearRenderer::createPerImageSemaphores()
{
    destroyPerImageSemaphores();

    VkSemaphoreCreateInfo sci{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    const size_t count = m_swapchain.images().size();
    m_renderFinishedPerImage.resize(count, VK_NULL_HANDLE);

    for (size_t i = 0; i < count; ++i)
        vk_check(vkCreateSemaphore(m_device, &sci, nullptr, &m_renderFinishedPerImage[i]),
                 "vkCreateSemaphore(renderFinishedPerImage)");
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

void ClearRenderer::recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex)
{
    VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vk_check(vkBeginCommandBuffer(cmd, &bi), "vkBeginCommandBuffer");

    VkImage img = m_swapchain.images()[imageIndex];
    const VkImageLayout old = m_swapchain.layoutOf(imageIndex);

    cmd_image_barrier(
        cmd,
        img,
        old,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT
    );

    VkClearColorValue color{};
    color.float32[0] = 0.05f;
    color.float32[1] = 0.05f;
    color.float32[2] = 0.08f;
    color.float32[3] = 1.0f;

    VkImageSubresourceRange range{};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0;
    range.levelCount = 1;
    range.baseArrayLayer = 0;
    range.layerCount = 1;

    vkCmdClearColorImage(cmd, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1, &range);

    cmd_image_barrier(
        cmd,
        img,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        0,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
    );

    vk_check(vkEndCommandBuffer(cmd), "vkEndCommandBuffer");
}

bool ClearRenderer::drawFrame(int fbWidth, int fbHeight)
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
        createPerImageSemaphores(); // важно
        return true;
    }
    vk_check(acq, "vkAcquireNextImageKHR");

    // renderFinished строго привязан к imageIndex
    VkSemaphore renderFinished = m_renderFinishedPerImage[imageIndex];

    vk_check(vkResetCommandBuffer(m_cmds[fi], 0), "vkResetCommandBuffer");
    recordCommandBuffer(m_cmds[fi], imageIndex);

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

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
        createPerImageSemaphores(); // важно
        return true;
    }
    vk_check(pres, "vkQueuePresentKHR");

    m_swapchain.setLayout(imageIndex, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    return false;
}
