#include "vk/VulkanUtils.h"

#include <iostream>
#include <cstring>
#include <unordered_set>
#include <GLFW/glfw3.h>

void vk_check(VkResult res, const char* what)
{
    if (res != VK_SUCCESS)
    {
        std::cerr << "[Vulkan] " << what << " failed, VkResult=" << res << "\n";
        std::exit(EXIT_FAILURE);
    }
}

bool has_validation_layer_support(const std::vector<const char*>& layers)
{
    uint32_t count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> props(count);
    vkEnumerateInstanceLayerProperties(&count, props.data());

    for (const char* wanted : layers)
    {
        bool found = false;
        for (const auto& lp : props)
        {
            if (std::strcmp(wanted, lp.layerName) == 0)
            {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    return true;
}

std::vector<const char*> get_required_instance_extensions(bool enableValidation)
{
    uint32_t glfwCount = 0;
    const char** glfwExt = glfwGetRequiredInstanceExtensions(&glfwCount);
    if (!glfwExt || glfwCount == 0)
    {
        std::cerr << "glfwGetRequiredInstanceExtensions returned nothing.\n";
        std::exit(EXIT_FAILURE);
    }

    std::vector<const char*> exts(glfwExt, glfwExt + glfwCount);

    if (enableValidation)
        exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

#if defined(__APPLE__)
    exts.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#endif

    return exts;
}

QueueFamilyIndices find_queue_families(VkPhysicalDevice phys, VkSurfaceKHR surface)
{
    QueueFamilyIndices out{};

    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &count, families.data());

    for (uint32_t i = 0; i < count; ++i)
    {
        const auto& q = families[i];

        if ((q.queueFlags & VK_QUEUE_GRAPHICS_BIT) && !out.graphicsFamily.has_value())
            out.graphicsFamily = i;

        VkBool32 present = VK_FALSE;
        vk_check(vkGetPhysicalDeviceSurfaceSupportKHR(phys, i, surface, &present),
                 "vkGetPhysicalDeviceSurfaceSupportKHR");
        if (present && !out.presentFamily.has_value())
            out.presentFamily = i;

        if (out.complete()) break;
    }

    return out;
}

bool device_supports_extensions(VkPhysicalDevice phys, const std::vector<const char*>& required)
{
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(phys, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> exts(count);
    vkEnumerateDeviceExtensionProperties(phys, nullptr, &count, exts.data());

    std::unordered_set<std::string> available;
    available.reserve(exts.size());
    for (const auto& e : exts)
        available.insert(e.extensionName);

    for (const char* r : required)
    {
        if (!available.contains(r))
            return false;
    }
    return true;
}

static bool has_device_extension(VkPhysicalDevice phys, const char* name)
{
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(phys, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> exts(count);
    vkEnumerateDeviceExtensionProperties(phys, nullptr, &count, exts.data());

    for (const auto& e : exts)
    {
        if (std::strcmp(e.extensionName, name) == 0)
            return true;
    }
    return false;
}

std::vector<const char*> get_device_extensions_for_mesh_text(
    VkPhysicalDevice phys,
    bool needSpirv14Fallback)
{
    std::vector<const char*> out;

    // ближайшие шаги потребуют swapchain
    out.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    // наш челлендж
    out.push_back(VK_EXT_MESH_SHADER_EXTENSION_NAME);

    // если Vulkan < 1.2, VK_EXT_mesh_shader допускает зависимость через VK_KHR_spirv_1_4
    // а VK_KHR_spirv_1_4 требует VK_KHR_shader_float_controls
    if (needSpirv14Fallback)
    {
        out.push_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME);
        out.push_back(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);
    }

#if defined(__APPLE__)
    // MoltenVK часто требует portability subset — включаем если есть.
    if (has_device_extension(phys, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME))
        out.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#endif

    return out;
}
