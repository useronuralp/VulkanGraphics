#include "Window.h"
#include <iostream>
#include "core.h"
namespace Anor
{
    void Window::glfw_error_callback(int error, const char* description)
    {
        fprintf(stderr, "Glfw Error %d: %s\n", error, description);
    }
	Window::Window(const char* windowName, uint32_t width, uint32_t height)
        :m_WindowName(windowName), m_Height(height), m_Width(width)
	{
        ASSERT(glfwInit(), "Failed to initialize GLFW!, glfwInit() fnc failed.");

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // Tell it to not use OpenGL as the default API.
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);   // Disable the resizing functionality.

        m_Window = glfwCreateWindow(m_Width, m_Height, "Vulkan Demo", nullptr, nullptr); // Return a pointer to the handle.
        
        glfwSetErrorCallback(glfw_error_callback);
        glfwSetWindowUserPointer(m_Window, this);

        ASSERT(m_Window, "Failed to create Window! 'm_Window' is empty.");
	}
    Window::~Window()
    {
        glfwDestroyWindow(m_Window);
        glfwTerminate();
    }
}