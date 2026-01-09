#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <optional>

struct QueueFamilyIndices
{
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool complete() const
    {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

void vk_check(VkResult res, const char* what);

bool has_validation_layer_support(const std::vector<const char*>& layers);

std::vector<const char*> get_required_instance_extensions(bool enableValidation);

QueueFamilyIndices find_queue_families(VkPhysicalDevice phys, VkSurfaceKHR surface);

bool device_supports_extensions(VkPhysicalDevice phys, const std::vector<const char*>& required);

std::vector<const char*> get_device_extensions_for_mesh_text(
    VkPhysicalDevice phys,
    bool needSpirv14Fallback // if device API < 1.2
);
