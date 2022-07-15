#pragma once
#include <GLFW/glfw3.h>
namespace Anor
{
	class Window
	{
	public:
		Window(const char* windowName, uint32_t width, uint32_t height);
		~Window();
		GLFWwindow* GetNativeWindow() { return m_Window; }
	private:
		static void glfw_error_callback(int error, const char* description);
	private:
		GLFWwindow* m_Window = nullptr;
		uint32_t	m_Height;
		uint32_t	m_Width;
		const char* m_WindowName;
	};
}