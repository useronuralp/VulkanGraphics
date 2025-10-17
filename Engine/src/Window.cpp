#include "core.h"
#include "Window.h"

#include <iostream>
uint32_t Window::GetHeight()
{
    int width;
    int height;
    glfwGetFramebufferSize(_Window, &width, &height);
    return height;
}
uint32_t Window::GetWidth()
{
    int width;
    int height;
    glfwGetFramebufferSize(_Window, &width, &height);
    return width;
}
void Window::OnUpdate()
{
}
void Window::ResetVariables()
{
    _MouseScrolled      = false;
    _MouseXScrollOffset = 0.0f, _MouseYScrollOffset = 0.0f;
}
std::pair<float, float> Window::GetMouseScrollOffset()
{
    return std::pair<float, float>(_MouseXScrollOffset, _MouseYScrollOffset);
}
GLFWwindow* Window::GetNativeWindow()
{
    return _Window;
}
bool Window::IsWindowResized()
{
    return _WindowResized;
}
bool Window::IsMouseScrolled()
{
    return _MouseScrolled;
}
void Window::OnResize()
{
    _WindowResized = false;
}
bool Window::ShouldClose()
{
    return glfwWindowShouldClose(_Window);
}
void Window::glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}
void Window::glfw_framebuffer_resize_callback(GLFWwindow* window, int width, int height)
{
    auto app            = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
    app->_WindowResized = true;
}
void Window::glfw_mouse_scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    auto app                 = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
    app->_MouseScrolled      = true;
    app->_MouseXScrollOffset = xoffset;
    app->_MouseYScrollOffset = yoffset;
}
Window::Window(const char* windowName, uint32_t width, uint32_t height) : _WindowName(windowName), _Height(height), _Width(width)
{
    ASSERT(glfwInit(), "Failed to initialize GLFW!, glfwInit() fnc failed.");

    glfwWindowHint(GLFW_CLIENT_API,
                   GLFW_NO_API); // Tell it to not use OpenGL as the default API.
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    _Window = glfwCreateWindow(_Width, _Height, "Vulkan Demo", nullptr,
                               nullptr); // Return a pointer to the handle.
    glfwSetWindowUserPointer(_Window, this);
    glfwSetFramebufferSizeCallback(_Window, glfw_framebuffer_resize_callback);
    glfwSetErrorCallback(glfw_error_callback);
    glfwSetScrollCallback(_Window, glfw_mouse_scroll_callback);

    ASSERT(_Window, "Failed to create Window! '_Window' is empty.");
}
Window::~Window()
{
    glfwDestroyWindow(_Window);
    glfwTerminate();
}
