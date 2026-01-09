#include "platform/Window.h"
#include "vk/VulkanContext.h"
#include "vk/Swapchain.h"
#include "vk/MeshTestPipeline.h"
#include "vk/MeshTestRenderer.h"

#include <thread>
#include <chrono>

int main()
{
    Window window(1280, 720, "MSDF Text (Mesh Shader Triangle)");

    VulkanContext vk(window.handle());

    int fbW = 0, fbH = 0;
    window.getFramebufferSize(fbW, fbH);
    while (fbW == 0 || fbH == 0)
    {
        window.pollEvents();
        window.getFramebufferSize(fbW, fbH);
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    Swapchain swapchain(
        vk.physicalDevice(),
        vk.device(),
        vk.surface(),
        vk.graphicsFamily(),
        vk.presentFamily(),
        fbW, fbH
    );

    MeshTestPipeline pipeline(vk.device(), swapchain.format());

    MeshTestRenderer renderer(
        vk.physicalDevice(),          // <-- добавили
        vk.device(),
        vk.graphicsQueue(),
        vk.presentQueue(),
        vk.graphicsFamily(),
        swapchain,
        pipeline,
        vk.vkCmdDrawMeshTasksEXT
    );

    while (!window.shouldClose())
    {
        window.pollEvents();
        window.getFramebufferSize(fbW, fbH);
        if (fbW == 0 || fbH == 0)
            continue;

        renderer.drawFrame(fbW, fbH);
    }

    return 0;
}
