#include "vk/VulkanContext.h"
#include "vk/VulkanUtils.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <iostream>
#include <set>
#include <cstring>

#ifndef NDEBUG
static constexpr bool kWantValidation = true;
#else
static constexpr bool kWantValidation = false;
#endif

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
    void*)
{
    const char* sev =
        (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) ? "ERROR" :
        (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) ? "WARN" :
        (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) ? "INFO" : "VERBOSE";

    std::cerr << "[Vulkan][" << sev << "] " << callbackData->pMessage << "\n";
    return VK_FALSE;
}

static VkDebugUtilsMessengerEXT create_debug_messenger(VkInstance instance)
{
    auto fnCreate = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkCreateDebugUtilsMessengerEXT");
    if (!fnCreate) return VK_NULL_HANDLE;

    VkDebugUtilsMessengerCreateInfoEXT ci{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
    ci.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    ci.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    ci.pfnUserCallback = debug_callback;

    VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
    vk_check(fnCreate(instance, &ci, nullptr, &messenger), "vkCreateDebugUtilsMessengerEXT");
    return messenger;
}

static void destroy_debug_messenger(VkInstance instance, VkDebugUtilsMessengerEXT messenger)
{
    if (!messenger) return;
    auto fnDestroy = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkDestroyDebugUtilsMessengerEXT");
    if (fnDestroy) fnDestroy(instance, messenger, nullptr);
}

VulkanContext::VulkanContext(GLFWwindow* window)
{
    m_validationLayers = { "VK_LAYER_KHRONOS_validation" };

    m_enableValidation = kWantValidation && has_validation_layer_support(m_validationLayers);
    if (kWantValidation && !m_enableValidation)
        std::cerr << "[Vulkan] Validation requested but VK_LAYER_KHRONOS_validation not found. Continuing without it.\n";

    createInstance();
    setupDebug();
    createSurface(window);
    pickPhysicalDevice();
    createDevice();
}

VulkanContext::~VulkanContext()
{
    if (m_device)
        vkDestroyDevice(m_device, nullptr);

    if (m_surface)
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);

    if (m_enableValidation)
        destroy_debug_messenger(m_instance, m_debugMessenger);

    if (m_instance)
        vkDestroyInstance(m_instance, nullptr);
}

void VulkanContext::createInstance()
{
    VkApplicationInfo appInfo{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
    appInfo.pApplicationName = "msdf-text-meshshader";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = "none";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    auto exts = get_required_instance_extensions(m_enableValidation);

    VkInstanceCreateInfo ci{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    ci.pApplicationInfo = &appInfo;
    ci.enabledExtensionCount = (uint32_t)exts.size();
    ci.ppEnabledExtensionNames = exts.data();

#if defined(__APPLE__)
    ci.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    VkDebugUtilsMessengerCreateInfoEXT dbgCreateInfo{};
    if (m_enableValidation)
    {
        ci.enabledLayerCount = (uint32_t)m_validationLayers.size();
        ci.ppEnabledLayerNames = m_validationLayers.data();

        dbgCreateInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
        dbgCreateInfo.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        dbgCreateInfo.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        dbgCreateInfo.pfnUserCallback = debug_callback;

        // чтобы ловить сообщения ещё во время vkCreateInstance
        ci.pNext = &dbgCreateInfo;
    }

    vk_check(vkCreateInstance(&ci, nullptr, &m_instance), "vkCreateInstance");
}

void VulkanContext::setupDebug()
{
    if (!m_enableValidation) return;
    m_debugMessenger = create_debug_messenger(m_instance);
}

void VulkanContext::createSurface(GLFWwindow* window)
{
    vk_check(glfwCreateWindowSurface(m_instance, window, nullptr, &m_surface),
             "glfwCreateWindowSurface");
}

bool VulkanContext::isDeviceSuitable(VkPhysicalDevice phys)
{
    // очереди
    auto q = find_queue_families(phys, m_surface);
    if (!q.complete())
        return false;

    // версия API девайса
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(phys, &props);
    const uint32_t api = props.apiVersion;

    const bool apiAtLeast12 =
        VK_API_VERSION_MAJOR(api) > 1 ||
        (VK_API_VERSION_MAJOR(api) == 1 && VK_API_VERSION_MINOR(api) >= 2);

    const bool needSpirv14Fallback = !apiAtLeast12;

    // необходимые device extensions под наш пайплайн (swapchain + mesh shader + (fallback))
    auto reqExts = get_device_extensions_for_mesh_text(phys, needSpirv14Fallback);
    if (!device_supports_extensions(phys, reqExts))
        return false;

    // mesh shader feature flags
    VkPhysicalDeviceMeshShaderFeaturesEXT meshFeat{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT };
    VkPhysicalDeviceFeatures2 feats2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
    feats2.pNext = &meshFeat;
    vkGetPhysicalDeviceFeatures2(phys, &feats2);

    if (!meshFeat.meshShader)
        return false;

    return true;
}

void VulkanContext::pickPhysicalDevice()
{
    uint32_t count = 0;
    vk_check(vkEnumeratePhysicalDevices(m_instance, &count, nullptr), "vkEnumeratePhysicalDevices(count)");
    if (count == 0)
    {
        std::cerr << "No Vulkan physical devices found.\n";
        std::exit(EXIT_FAILURE);
    }

    std::vector<VkPhysicalDevice> devices(count);
    vk_check(vkEnumeratePhysicalDevices(m_instance, &count, devices.data()), "vkEnumeratePhysicalDevices");

    // простой скоринг: дискретка + taskShader предпочтительнее
    int bestScore = -1;
    VkPhysicalDevice best = VK_NULL_HANDLE;

    for (auto phys : devices)
    {
        if (!isDeviceSuitable(phys))
            continue;

        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(phys, &props);

        int score = 0;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            score += 1000;

        VkPhysicalDeviceMeshShaderFeaturesEXT meshFeat{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT };
        VkPhysicalDeviceFeatures2 feats2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
        feats2.pNext = &meshFeat;
        vkGetPhysicalDeviceFeatures2(phys, &feats2);

        if (meshFeat.taskShader)
            score += 100;

        if (score > bestScore)
        {
            bestScore = score;
            best = phys;
        }
    }

    if (!best)
    {
        std::cerr << "No suitable GPU found (need VK_EXT_mesh_shader + presentation support).\n";
        std::exit(EXIT_FAILURE);
    }

    m_physicalDevice = best;

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(m_physicalDevice, &props);
    std::cout << "Selected GPU: " << props.deviceName
              << " (API " << VK_API_VERSION_MAJOR(props.apiVersion) << "."
              << VK_API_VERSION_MINOR(props.apiVersion) << "."
              << VK_API_VERSION_PATCH(props.apiVersion) << ")\n";

    auto q = find_queue_families(m_physicalDevice, m_surface);
    m_graphicsFamily = q.graphicsFamily.value();
    m_presentFamily = q.presentFamily.value();
}

void VulkanContext::createDevice()
{
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(m_physicalDevice, &props);

    const uint32_t api = props.apiVersion;
    const bool apiAtLeast12 =
        VK_API_VERSION_MAJOR(api) > 1 ||
        (VK_API_VERSION_MAJOR(api) == 1 && VK_API_VERSION_MINOR(api) >= 2);

    const bool needSpirv14Fallback = !apiAtLeast12;
    auto deviceExts = get_device_extensions_for_mesh_text(m_physicalDevice, needSpirv14Fallback);

    std::set<uint32_t> uniqueFamilies = { m_graphicsFamily, m_presentFamily };
    std::vector<VkDeviceQueueCreateInfo> queues;
    queues.reserve(uniqueFamilies.size());

    float priority = 1.0f;
    for (uint32_t fam : uniqueFamilies)
    {
        VkDeviceQueueCreateInfo qci{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
        qci.queueFamilyIndex = fam;
        qci.queueCount = 1;
        qci.pQueuePriorities = &priority;
        queues.push_back(qci);
    }

    // Узнаём, что реально поддерживается, и включаем то, что нам нужно
    VkPhysicalDeviceMeshShaderFeaturesEXT supportedMeshFeat{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT };
    VkPhysicalDeviceVulkan13Features supported13{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
    supported13.pNext = &supportedMeshFeat;

    VkPhysicalDeviceFeatures2 supported2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
    supported2.pNext = &supported13;

    vkGetPhysicalDeviceFeatures2(m_physicalDevice, &supported2);


    VkPhysicalDeviceMeshShaderFeaturesEXT meshFeat{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT };
    meshFeat.meshShader = VK_TRUE;
    meshFeat.taskShader = supportedMeshFeat.taskShader ? VK_TRUE : VK_FALSE;

    VkPhysicalDeviceVulkan13Features v13{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
    v13.dynamicRendering = supported13.dynamicRendering ? VK_TRUE : VK_FALSE; // must-have для шага 5
    v13.maintenance4     = supported13.maintenance4 ? VK_TRUE : VK_FALSE;
    v13.pNext = &meshFeat;

    VkPhysicalDeviceFeatures2 feats2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
    feats2.features = {};
    feats2.pNext = &v13;

    VkDeviceCreateInfo dci{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    dci.queueCreateInfoCount = (uint32_t)queues.size();
    dci.pQueueCreateInfos = queues.data();
    dci.enabledExtensionCount = (uint32_t)deviceExts.size();
    dci.ppEnabledExtensionNames = deviceExts.data();
    dci.pEnabledFeatures = nullptr;
    dci.pNext = &feats2;

    if (m_enableValidation)
    {
        dci.enabledLayerCount = (uint32_t)m_validationLayers.size();
        dci.ppEnabledLayerNames = m_validationLayers.data();
    }

    vk_check(vkCreateDevice(m_physicalDevice, &dci, nullptr, &m_device), "vkCreateDevice");

    vkGetDeviceQueue(m_device, m_graphicsFamily, 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, m_presentFamily, 0, &m_presentQueue);

    // Подготовим функцию mesh draw на будущее (пока не используем)
    vkCmdDrawMeshTasksEXT = (PFN_vkCmdDrawMeshTasksEXT)vkGetDeviceProcAddr(m_device, "vkCmdDrawMeshTasksEXT");

    if (!supported13.dynamicRendering)
    {
        std::cerr << "GPU does not support dynamicRendering feature.\n";
        std::exit(EXIT_FAILURE);
    }

    std::cout << "Device created. MeshShader="
              << (supportedMeshFeat.meshShader ? "YES" : "NO")
              << ", TaskShader="
              << (supportedMeshFeat.taskShader ? "YES" : "NO")
              << "\n";
}
