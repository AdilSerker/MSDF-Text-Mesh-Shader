#include "vk/Texture2D.h"
#include "vk/VulkanUtils.h"

#include <iostream>
#include <cstring>
#include <utility>

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

Texture2D::~Texture2D() { destroy(); }

Texture2D::Texture2D(Texture2D&& rhs) noexcept { *this = std::move(rhs); }

Texture2D& Texture2D::operator=(Texture2D&& rhs) noexcept
{
    if (this == &rhs) return *this;
    destroy();
    m_phys = rhs.m_phys; rhs.m_phys = VK_NULL_HANDLE;
    m_device = rhs.m_device; rhs.m_device = VK_NULL_HANDLE;
    m_image = rhs.m_image; rhs.m_image = VK_NULL_HANDLE;
    m_mem = rhs.m_mem; rhs.m_mem = VK_NULL_HANDLE;
    m_view = rhs.m_view; rhs.m_view = VK_NULL_HANDLE;
    m_sampler = rhs.m_sampler; rhs.m_sampler = VK_NULL_HANDLE;
    m_width = rhs.m_width; rhs.m_width = 0;
    m_height = rhs.m_height; rhs.m_height = 0;
    m_format = rhs.m_format; rhs.m_format = VK_FORMAT_UNDEFINED;
    return *this;
}

void Texture2D::destroy()
{
    if (!m_device) return;

    if (m_sampler) vkDestroySampler(m_device, m_sampler, nullptr);
    if (m_view) vkDestroyImageView(m_device, m_view, nullptr);
    if (m_image) vkDestroyImage(m_device, m_image, nullptr);
    if (m_mem) vkFreeMemory(m_device, m_mem, nullptr);

    m_sampler = VK_NULL_HANDLE;
    m_view = VK_NULL_HANDLE;
    m_image = VK_NULL_HANDLE;
    m_mem = VK_NULL_HANDLE;
    m_device = VK_NULL_HANDLE;
    m_phys = VK_NULL_HANDLE;
}

void Texture2D::createFromRGBA8(
    VkPhysicalDevice phys,
    VkDevice device,
    VkCommandPool cmdPool,
    VkQueue graphicsQueue,
    uint32_t width,
    uint32_t height,
    const std::vector<uint8_t>& rgba,
    VkFormat format)
{
    destroy();

    if (rgba.size() != (size_t)width * (size_t)height * 4u) {
        std::cerr << "RGBA size mismatch: got " << rgba.size()
                  << ", expected " << (size_t)width * (size_t)height * 4u << "\n";
        std::exit(EXIT_FAILURE);
    }

    m_phys = phys;
    m_device = device;
    m_width = width;
    m_height = height;
    m_format = format;

    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory stagingMem = VK_NULL_HANDLE;
    VkDeviceSize byteSize = (VkDeviceSize)rgba.size();

    create_buffer(phys, device, byteSize,
                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  staging, stagingMem);

    void* mapped = nullptr;
    vk_check(vkMapMemory(device, stagingMem, 0, byteSize, 0, &mapped), "vkMapMemory");
    std::memcpy(mapped, rgba.data(), (size_t)byteSize);
    vkUnmapMemory(device, stagingMem);

    create_image(phys, device, width, height, format,
                 VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                 m_image, m_mem);

    // one-time cmd
    VkCommandBufferAllocateInfo ai{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    ai.commandPool = cmdPool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    vk_check(vkAllocateCommandBuffers(device, &ai, &cmd), "vkAllocateCommandBuffers(upload)");

    VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vk_check(vkBeginCommandBuffer(cmd, &bi), "vkBeginCommandBuffer(upload)");

    cmd_image_barrier(cmd, m_image,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        0, VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = { width, height, 1 };

    vkCmdCopyBufferToImage(cmd, staging, m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    cmd_image_barrier(cmd, m_image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    vk_check(vkEndCommandBuffer(cmd), "vkEndCommandBuffer(upload)");

    VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;

    vk_check(vkQueueSubmit(graphicsQueue, 1, &submit, VK_NULL_HANDLE), "vkQueueSubmit(upload)");
    vk_check(vkQueueWaitIdle(graphicsQueue), "vkQueueWaitIdle(upload)");

    vkFreeCommandBuffers(device, cmdPool, 1, &cmd);

    vkDestroyBuffer(device, staging, nullptr);
    vkFreeMemory(device, stagingMem, nullptr);

    VkImageViewCreateInfo vci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    vci.image = m_image;
    vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vci.format = format;
    vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vci.subresourceRange.levelCount = 1;
    vci.subresourceRange.layerCount = 1;
    vk_check(vkCreateImageView(device, &vci, nullptr, &m_view), "vkCreateImageView(texture)");

    VkSamplerCreateInfo sci{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    sci.magFilter = VK_FILTER_LINEAR;
    sci.minFilter = VK_FILTER_LINEAR;
    sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.maxLod = 0.0f;
    vk_check(vkCreateSampler(device, &sci, nullptr, &m_sampler), "vkCreateSampler(texture)");
}
