#include "platform/Window.h"

#include <iostream>
#include <cstdlib>

static void glfw_error_callback(int error, const char* description)
{
    std::cerr << "[GLFW] Error " << error << ": " << (description ? description : "") << "\n";
}

Window::Window(int width, int height, const char* title)
{
    glfwSetErrorCallback(glfw_error_callback);

    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW.\n";
        std::exit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!m_window)
    {
        std::cerr << "Failed to create GLFW window.\n";
        glfwTerminate();
        std::exit(EXIT_FAILURE);
    }
}

Window::~Window()
{
    if (m_window)
        glfwDestroyWindow(m_window);

    glfwTerminate();
}

bool Window::shouldClose() const
{
    return glfwWindowShouldClose(m_window) != 0;
}

void Window::pollEvents() const
{
    glfwPollEvents();
}

void Window::getFramebufferSize(int& w, int& h) const
{
    glfwGetFramebufferSize(m_window, &w, &h);
}
