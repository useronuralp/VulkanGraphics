#pragma once
#include <GLFW/glfw3.h>
#include <utility>
namespace OVK
{
	class Window
	{
	public:
		//Constructors / Destructors
		Window(const char* windowName, uint32_t width, uint32_t height);
		~Window();
	public:
		GLFWwindow* GetNativeWindow() { return m_Window; }
		uint32_t	GetHeight();
		uint32_t	GetWidth();
		bool		IsWindowResized() { return m_WindowResized; }
		bool		IsMouseScrolled() { return m_MouseScrolled; }
		void		OnUpdate();
		void		OnResize()		  { m_WindowResized = false; };
		std::pair<float, float> GetMouseScrollOffset();
	private:
		// GLFW callbacks.
		static void glfw_error_callback(int error, const char* description);
		static void glfw_framebuffer_resize_callback(GLFWwindow* window, int width, int height);
		static void glfw_mouse_scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
	private:
		GLFWwindow* m_Window = nullptr;
		uint32_t	m_Height;
		uint32_t	m_Width;
		const char* m_WindowName;
		double		m_MouseXScrollOffset = 0.0f;
		double		m_MouseYScrollOffset = 0.0f;

		bool		m_WindowResized = false;
		bool		m_MouseScrolled = false;
	};
}