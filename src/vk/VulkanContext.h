#pragma once

#include <vulkan/vulkan.h>
#include <vector>

struct GLFWwindow;

class VulkanContext
{
public:
    explicit VulkanContext(GLFWwindow* window);
    ~VulkanContext();

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    VkInstance instance() const { return m_instance; }
    VkPhysicalDevice physicalDevice() const { return m_physicalDevice; }
    VkDevice device() const { return m_device; }
    VkSurfaceKHR surface() const { return m_surface; }

    uint32_t graphicsFamily() const { return m_graphicsFamily; }
    uint32_t presentFamily() const { return m_presentFamily; }

    VkQueue graphicsQueue() const { return m_graphicsQueue; }
    VkQueue presentQueue() const { return m_presentQueue; }

    // На будущее (mesh draw)
    PFN_vkCmdDrawMeshTasksEXT vkCmdDrawMeshTasksEXT = nullptr;

private:
    void createInstance();
    void setupDebug();
    void createSurface(GLFWwindow* window);
    void pickPhysicalDevice();
    void createDevice();

    bool isDeviceSuitable(VkPhysicalDevice phys);

private:
    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;

    VkSurfaceKHR m_surface = VK_NULL_HANDLE;

    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;

    uint32_t m_graphicsFamily = UINT32_MAX;
    uint32_t m_presentFamily = UINT32_MAX;

    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;

    bool m_enableValidation = false;

    std::vector<const char*> m_validationLayers;
};
