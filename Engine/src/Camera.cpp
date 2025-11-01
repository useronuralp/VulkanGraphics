#include "Camera.h"
#include "EngineInternal.h"
#include "VulkanContext.h"
#include "Window.h"
Camera::Camera(float fov, float aspectRatio) : _FOV(fov), _AspectRatio(aspectRatio)
{
    UpdateProjection();
    UpdateView();
}

void Camera::OnUpdate(float deltaTime)
{
    auto window = EngineInternal::GetContext().GetWindow()->GetNativeWindow();

    const glm::vec2& mouse{ GetMouseXOffset(), GetMouseYOffset() };
    glm::vec2        delta = (mouse - _InitialMousePosition) * 0.003f;
    _InitialMousePosition  = mouse;

    if (IsMouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT))
    {
        MousePan(delta, deltaTime);
    }
    if (IsMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT))
    {
        MouseRotate(delta, deltaTime);
        // MousePan(delta, deltaTime);
    }
    if (EngineInternal::GetContext().GetWindow()->IsMouseScrolled())
    {
        auto [x, y] = EngineInternal::GetContext().GetWindow()->GetMouseScrollOffset();
        OnMouseScroll(x, y, deltaTime);
        EngineInternal::GetContext().GetWindow()->ResetVariables(); // Resets mouse scroll variables for now.
    }

    UpdateView();
}

void Camera::UpdateProjection()
{
    _AspectRatio          = _ViewportWidth / _ViewportHeight;
    _ProjectionMatrix     = glm::perspective(glm::radians(_FOV), _AspectRatio, _NearClip, _FarClip);
    _ViewProjectionMatrix = _ProjectionMatrix * _ViewMatrix;
}

void Camera::UpdateView()
{
    _Position             = CalculatePosition();

    glm::quat orientation = GetOrientation();
    _ViewMatrix           = glm::translate(glm::mat4(1.0f), _Position) * glm::toMat4(orientation);
    _ViewMatrix           = glm::inverse(_ViewMatrix);
    _ViewProjectionMatrix = _ProjectionMatrix * _ViewMatrix;
}

void Camera::MousePan(const glm::vec2& delta, float deltaTime)
{
    float zoomSpeed        = 4.0f;
    auto [xSpeed, ySpeed]  = PanSpeed();
    glm::vec3 modifierx    = glm::vec3(0, 0, 0);
    glm::vec3 modifiery    = glm::vec3(0, 0, 0);
    glm::vec3 zoomModifier = glm::vec3(0, 0, 0);

    if (IsKeyDown(GLFW_KEY_SPACE))
    {
        modifiery = GetUpDirection() * xSpeed * _Distance * deltaTime * 10.0f;
    }
    else if (IsKeyDown(GLFW_KEY_LEFT_CONTROL))
    {
        modifiery = -GetUpDirection() * xSpeed * _Distance * deltaTime * 10.0f;
    }

    if (IsKeyDown(GLFW_KEY_A))
    {
        modifierx = -GetRightDirection() * xSpeed * _Distance * deltaTime * 10.0f;
    }
    else if (IsKeyDown(GLFW_KEY_D))
    {
        modifierx = GetRightDirection() * xSpeed * _Distance * deltaTime * 10.0f;
    }

    if (IsKeyDown(GLFW_KEY_W))
    {
        zoomModifier = normalize(GetForwardDirection()) * deltaTime * 10.0f;
    }
    else if (IsKeyDown(GLFW_KEY_S))
    {
        zoomModifier = -normalize(GetForwardDirection()) * deltaTime * 10.0f;
    }

    _FocalPoint += modifierx + -GetRightDirection() * delta.x * xSpeed * _Distance;
    _FocalPoint += modifiery + GetUpDirection() * delta.y * xSpeed * _Distance;
    _FocalPoint -= zoomModifier;
}

void Camera::MouseRotate(const glm::vec2& delta, float deltaTime)
{
    float yawSign = GetUpDirection().y < 0 ? -1.0f : 1.0f;
    _Yaw += yawSign * delta.x * RotationSpeed() * deltaTime * 30;
    _Pitch += delta.y * RotationSpeed() * deltaTime * 30;
}

void Camera::MouseZoom(float delta, float deltaTime)
{
    float zoomSpeed = 4.0f;
    if (IsKeyDown(GLFW_KEY_LEFT_CONTROL))
    {
        zoomSpeed = 20.0f;
    }
    _FocalPoint -= normalize(GetForwardDirection()) * delta * zoomSpeed * deltaTime * 10.0f;
}

glm::vec3 Camera::CalculatePosition() const
{
    return _FocalPoint + GetForwardDirection() * _Distance;
}

std::pair<float, float> Camera::PanSpeed() const
{
    float x       = std::min(_ViewportWidth / 1000.0f, 2.4f); // max = 2.4f
    float xFactor = 0.0366f * (x * x) - 0.1778f * x + 0.3021f;

    float y       = std::min(_ViewportHeight / 1000.0f, 2.4f); // max = 2.4f
    float yFactor = 0.0366f * (y * y) - 0.1778f * y + 0.3021f;

    return { xFactor, yFactor };
}

float Camera::RotationSpeed() const
{
    return 0.8f;
}

float Camera::ZoomSpeed() const
{
    float distance = _Distance * 0.2f;
    distance       = std::max(distance, 0.0f);
    float speed    = distance * distance;
    speed          = std::min(speed, 100.0f); // max speed = 100
    return speed;
}

void Camera::OnMouseScroll(float X, float Y, float deltaTime)
{
    float delta = Y * 0.05f;
    MouseZoom(delta, deltaTime);
    UpdateView();
}

bool Camera::IsKeyDown(int keyCode)
{
    return glfwGetKey(EngineInternal::GetContext().GetWindow()->GetNativeWindow(),
                      keyCode); // GLFW_RELEASE equals to 0 thats why this works.
}

bool Camera::IsMouseButtonDown(int keyCode)
{
    return glfwGetMouseButton(EngineInternal::GetContext().GetWindow()->GetNativeWindow(), keyCode);
}

double Camera::GetMouseXOffset()
{
    double xpos, ypos;
    glfwGetCursorPos(EngineInternal::GetContext().GetWindow()->GetNativeWindow(), &xpos, &ypos);
    return xpos;
}

double Camera::GetMouseYOffset()
{
    double xpos, ypos;
    glfwGetCursorPos(EngineInternal::GetContext().GetWindow()->GetNativeWindow(), &xpos, &ypos);
    return ypos;
}
glm::vec3 Camera::GetUpDirection() const
{
    return glm::rotate(GetOrientation(), glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::vec3 Camera::GetRightDirection() const
{
    return glm::rotate(GetOrientation(), glm::vec3(1.0f, 0.0f, 0.0f));
}

glm::vec3 Camera::GetForwardDirection() const
{
    return glm::rotate(GetOrientation(), glm::vec3(0.0f, 0.0f, 1.0f));
}

glm::quat Camera::GetOrientation() const
{
    return glm::quat(glm::vec3(-_Pitch, -_Yaw, 0.0f));
}

float Camera::GetDistance() const
{
    return _Distance;
}
glm::vec3 Camera::GetPosition() const
{
    return _Position;
}
float Camera::GetPitch() const
{
    return _Pitch;
}
float Camera::GetYaw() const
{
    return _Yaw;
}
glm::mat4 Camera::GetProjectionMatrix() const
{
    return _ProjectionMatrix;
}
glm::mat4 Camera::GetViewMatrix() const
{
    return _ViewMatrix;
}
glm::mat4 Camera::GetViewProjectionMatrix() const
{
    return _ViewProjectionMatrix;
}

float Camera::GetNearClip() const
{
    return _NearClip;
}

float Camera::GetFarClip() const
{
    return _FarClip;
}
void Camera::SetDistance(float distance)
{
    _Distance = distance;
}
void Camera::SetViewportSize(float width, float height)
{
    _ViewportWidth = width, _ViewportHeight = height;
    UpdateProjection();
}