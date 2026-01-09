#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>
#include <cassert>

class Swapchain
{
public:
    Swapchain(
        VkPhysicalDevice phys,
        VkDevice device,
        VkSurfaceKHR surface,
        uint32_t graphicsFamily,
        uint32_t presentFamily,
        int fbWidth,
        int fbHeight);

    ~Swapchain();

    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    void recreate(int fbWidth, int fbHeight);

    VkSwapchainKHR handle() const { return m_swapchain; }
    VkFormat format() const { return m_format; }
    VkExtent2D extent() const { return m_extent; }

    // ✅ методы, которых не хватало для MeshTestRenderer
    uint32_t imageCount() const { return (uint32_t)m_images.size(); }

    VkImage image(uint32_t i) const {
        assert(i < m_images.size());
        return m_images[i];
    }

    VkImageView imageView(uint32_t i) const {
        assert(i < m_imageViews.size());
        return m_imageViews[i];
    }

    const std::vector<VkImage>& images() const { return m_images; }
    const std::vector<VkImageView>& imageViews() const { return m_imageViews; }

    VkImageLayout layoutOf(uint32_t imageIndex) const {
        assert(imageIndex < m_imageLayouts.size());
        return m_imageLayouts[imageIndex];
    }

    void setLayout(uint32_t imageIndex, VkImageLayout layout) {
        assert(imageIndex < m_imageLayouts.size());
        m_imageLayouts[imageIndex] = layout;
    }

private:
    void create(int fbWidth, int fbHeight);
    void destroy();

private:
    VkPhysicalDevice m_phys = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;

    uint32_t m_graphicsFamily = 0;
    uint32_t m_presentFamily = 0;

    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkFormat m_format = VK_FORMAT_UNDEFINED;
    VkExtent2D m_extent{};

    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_imageViews;
    std::vector<VkImageLayout> m_imageLayouts;
};
