#include "vk/Swapchain.h"
#include "vk/VulkanUtils.h"

#include <algorithm>
#include <iostream>

static VkSurfaceFormatKHR choose_format(const std::vector<VkSurfaceFormatKHR>& formats)
{
    for (const auto& f : formats)
    {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return f;
    }
    return formats.front();
}

static VkPresentModeKHR choose_present_mode(const std::vector<VkPresentModeKHR>& modes)
{
    for (auto m : modes)
    {
        if (m == VK_PRESENT_MODE_MAILBOX_KHR)
            return m;
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D choose_extent(const VkSurfaceCapabilitiesKHR& caps, int fbWidth, int fbHeight)
{
    if (caps.currentExtent.width != UINT32_MAX)
        return caps.currentExtent;

    VkExtent2D e{};
    e.width  = (uint32_t)std::clamp(fbWidth,  (int)caps.minImageExtent.width,  (int)caps.maxImageExtent.width);
    e.height = (uint32_t)std::clamp(fbHeight, (int)caps.minImageExtent.height, (int)caps.maxImageExtent.height);
    return e;
}

Swapchain::Swapchain(
    VkPhysicalDevice phys,
    VkDevice device,
    VkSurfaceKHR surface,
    uint32_t graphicsFamily,
    uint32_t presentFamily,
    int fbWidth,
    int fbHeight)
    : m_phys(phys)
    , m_device(device)
    , m_surface(surface)
    , m_graphicsFamily(graphicsFamily)
    , m_presentFamily(presentFamily)
{
    create(fbWidth, fbHeight);
}

Swapchain::~Swapchain()
{
    destroy();
}

void Swapchain::recreate(int fbWidth, int fbHeight)
{
    destroy();
    create(fbWidth, fbHeight);
}

void Swapchain::create(int fbWidth, int fbHeight)
{
    VkSurfaceCapabilitiesKHR caps{};
    vk_check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_phys, m_surface, &caps),
             "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

    uint32_t formatCount = 0;
    vk_check(vkGetPhysicalDeviceSurfaceFormatsKHR(m_phys, m_surface, &formatCount, nullptr),
             "vkGetPhysicalDeviceSurfaceFormatsKHR(count)");
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vk_check(vkGetPhysicalDeviceSurfaceFormatsKHR(m_phys, m_surface, &formatCount, formats.data()),
             "vkGetPhysicalDeviceSurfaceFormatsKHR");

    uint32_t presentCount = 0;
    vk_check(vkGetPhysicalDeviceSurfacePresentModesKHR(m_phys, m_surface, &presentCount, nullptr),
             "vkGetPhysicalDeviceSurfacePresentModesKHR(count)");
    std::vector<VkPresentModeKHR> presents(presentCount);
    vk_check(vkGetPhysicalDeviceSurfacePresentModesKHR(m_phys, m_surface, &presentCount, presents.data()),
             "vkGetPhysicalDeviceSurfacePresentModesKHR");

    const VkSurfaceFormatKHR chosenFormat = choose_format(formats);
    const VkPresentModeKHR chosenPresent = choose_present_mode(presents);
    m_extent = choose_extent(caps, fbWidth, fbHeight);
    m_format = chosenFormat.format;

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
        imageCount = caps.maxImageCount;

    VkSwapchainCreateInfoKHR ci{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    ci.surface = m_surface;
    ci.minImageCount = imageCount;
    ci.imageFormat = chosenFormat.format;
    ci.imageColorSpace = chosenFormat.colorSpace;
    ci.imageExtent = m_extent;
    ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queueFamilyIndices[] = { m_graphicsFamily, m_presentFamily };
    if (m_graphicsFamily != m_presentFamily)
    {
        ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        ci.queueFamilyIndexCount = 2;
        ci.pQueueFamilyIndices = queueFamilyIndices;
    }
    else
    {
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    ci.preTransform = caps.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode = chosenPresent;
    ci.clipped = VK_TRUE;

    vk_check(vkCreateSwapchainKHR(m_device, &ci, nullptr, &m_swapchain), "vkCreateSwapchainKHR");

    uint32_t swapImgCount = 0;
    vk_check(vkGetSwapchainImagesKHR(m_device, m_swapchain, &swapImgCount, nullptr),
             "vkGetSwapchainImagesKHR(count)");
    m_images.resize(swapImgCount);
    vk_check(vkGetSwapchainImagesKHR(m_device, m_swapchain, &swapImgCount, m_images.data()),
             "vkGetSwapchainImagesKHR");

    m_imageViews.resize(swapImgCount);
    for (uint32_t i = 0; i < swapImgCount; ++i)
    {
        VkImageViewCreateInfo vci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        vci.image = m_images[i];
        vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vci.format = m_format;
        vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vci.subresourceRange.baseMipLevel = 0;
        vci.subresourceRange.levelCount = 1;
        vci.subresourceRange.baseArrayLayer = 0;
        vci.subresourceRange.layerCount = 1;

        vk_check(vkCreateImageView(m_device, &vci, nullptr, &m_imageViews[i]), "vkCreateImageView");
    }

    // ✅ Лучше стартовать с PRESENT_SRC_KHR, чтобы барьеры "PRESENT -> COLOR" не спорили с трекингом
    // Если захочешь супер-строго, можно стартовать с UNDEFINED и в renderer делать спец-ветку для первого кадра.
    m_imageLayouts.assign(swapImgCount, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    std::cout << "Swapchain created: " << swapImgCount
              << " images, extent " << m_extent.width << "x" << m_extent.height << "\n";
}

void Swapchain::destroy()
{
    for (auto v : m_imageViews)
        vkDestroyImageView(m_device, v, nullptr);
    m_imageViews.clear();

    if (m_swapchain)
        vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);

    m_swapchain = VK_NULL_HANDLE;
    m_images.clear();
    m_imageLayouts.clear();
}
