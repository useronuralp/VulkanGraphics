#pragma once
#include <GLFW/glfw3.h>
#include <utility>
class Window {
   public:
    // Constructors / Destructors
    Window(const char* windowName, uint32_t width, uint32_t height);
    ~Window();

   public:
    GLFWwindow*             GetNativeWindow();
    uint32_t                GetHeight();
    uint32_t                GetWidth();
    bool                    IsWindowResized();
    bool                    IsMouseScrolled();
    void                    OnResize();
    void                    OnUpdate();
    void                    ResetVariables();
    std::pair<float, float> GetMouseScrollOffset();

    bool ShouldClose();

   private:
    // GLFW callbacks.
    static void glfw_error_callback(int error, const char* description);
    static void glfw_framebuffer_resize_callback(GLFWwindow* window, int width, int height);
    static void glfw_mouse_scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

   private:
    GLFWwindow* _Window = nullptr;
    uint32_t    _Height;
    uint32_t    _Width;
    const char* _WindowName;
    double      _MouseXScrollOffset = 0.0f;
    double      _MouseYScrollOffset = 0.0f;

    bool _WindowResized             = false;
    bool _MouseScrolled             = false;
};
