#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>

class Texture2D
{
public:
    Texture2D() = default;
    ~Texture2D();

    Texture2D(const Texture2D&) = delete;
    Texture2D& operator=(const Texture2D&) = delete;

    Texture2D(Texture2D&& rhs) noexcept;
    Texture2D& operator=(Texture2D&& rhs) noexcept;

    void createFromRGBA8(
        VkPhysicalDevice phys,
        VkDevice device,
        VkCommandPool cmdPool,
        VkQueue graphicsQueue,
        uint32_t width,
        uint32_t height,
        const std::vector<uint8_t>& rgba,
        VkFormat format = VK_FORMAT_R8G8B8A8_UNORM);

    void destroy();

    VkImageView view() const { return m_view; }
    VkSampler sampler() const { return m_sampler; }

private:
    VkPhysicalDevice m_phys = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;

    VkImage m_image = VK_NULL_HANDLE;
    VkDeviceMemory m_mem = VK_NULL_HANDLE;
    VkImageView m_view = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;

    uint32_t m_width = 0, m_height = 0;
    VkFormat m_format = VK_FORMAT_UNDEFINED;
};
