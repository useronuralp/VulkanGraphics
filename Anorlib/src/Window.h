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
		uint32_t GetHeight();
		uint32_t GetWidth();
		bool IsWindowResized() { return m_WindowResized; }
		void ResetResize() { m_WindowResized = false; }
	private:
		static void glfw_error_callback(int error, const char* description);
		static void glfw_framebuffer_resize_callback(GLFWwindow* window, int width, int height);
	private:
		GLFWwindow* m_Window = nullptr;
		uint32_t	m_Height;
		uint32_t	m_Width;
		const char* m_WindowName;

		bool		m_WindowResized = false;
	};
}