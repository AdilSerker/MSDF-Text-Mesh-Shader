#include "platform/Window.h"
#include "vk/VulkanContext.h"
#include "vk/Swapchain.h"
#include "vk/MeshTestPipeline.h"
#include "vk/MeshTestRenderer.h"

#include <thread>
#include <chrono>
#include <memory>

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

    auto renderer = std::make_unique<MeshTestRenderer>(
        vk.physicalDevice(),
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

        // drawFrame() возвращает false если swapchain out-of-date/suboptimal
        if (!renderer->drawFrame())
        {
            vkDeviceWaitIdle(vk.device());

            swapchain.recreate(fbW, fbH);
            pipeline.recreate(swapchain.format());

            // Самый простой способ: пересоздать renderer (т.к. он хранит командные буферы/синхру по количеству images)
            renderer = std::make_unique<MeshTestRenderer>(
                vk.physicalDevice(),
                vk.device(),
                vk.graphicsQueue(),
                vk.presentQueue(),
                vk.graphicsFamily(),
                swapchain,
                pipeline,
                vk.vkCmdDrawMeshTasksEXT
            );
        }
    }

    vkDeviceWaitIdle(vk.device());
    return 0;
}
