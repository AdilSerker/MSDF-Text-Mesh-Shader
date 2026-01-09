#pragma once

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

class Window
{
public:
    Window(int width, int height, const char* title);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    GLFWwindow* handle() const { return m_window; }
    bool shouldClose() const;
    void pollEvents() const;

    void getFramebufferSize(int& w, int& h) const;

private:
    GLFWwindow* m_window = nullptr;
};
