#include "Camera.h"
#include "EngineInternal.h"
#include "VulkanContext.h"
#include "Window.h"
Camera::Camera(float fov, float aspectRatio) : m_FOV(fov), m_AspectRatio(aspectRatio)
{
    UpdateProjection();
    UpdateView();
}

void Camera::OnUpdate(float deltaTime)
{
    auto window = EngineInternal::GetContext().GetWindow()->GetNativeWindow();

    const glm::vec2& mouse{ GetMouseXOffset(), GetMouseYOffset() };
    glm::vec2        delta = (mouse - m_InitialMousePosition) * 0.003f;
    m_InitialMousePosition = mouse;

    if (IsMouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT))
        MousePan(delta);
    else if (IsMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT))
        MouseRotate(delta);
    else if (EngineInternal::GetContext().GetWindow()->IsMouseScrolled())
    {
        auto [x, y] = EngineInternal::GetContext().GetWindow()->GetMouseScrollOffset();
        OnMouseScroll(x, y);
        EngineInternal::GetContext().GetWindow()->ResetVariables(); // Resets mouse scroll variables for now.
    }
    UpdateView();
}

void Camera::UpdateProjection()
{
    m_AspectRatio          = m_ViewportWidth / m_ViewportHeight;
    m_ProjectionMatrix     = glm::perspective(glm::radians(m_FOV), m_AspectRatio, m_NearClip, m_FarClip);
    m_ViewProjectionMatrix = m_ProjectionMatrix * m_ViewMatrix;
}

void Camera::UpdateView()
{
    m_Position             = CalculatePosition();

    glm::quat orientation  = GetOrientation();
    m_ViewMatrix           = glm::translate(glm::mat4(1.0f), m_Position) * glm::toMat4(orientation);
    m_ViewMatrix           = glm::inverse(m_ViewMatrix);
    m_ViewProjectionMatrix = m_ProjectionMatrix * m_ViewMatrix;
}

void Camera::MousePan(const glm::vec2& delta)
{
    auto [xSpeed, ySpeed] = PanSpeed();
    if (IsKeyDown(GLFW_KEY_LEFT_CONTROL))
    {
        xSpeed *= 10.0f;
        ySpeed *= 10.0f;
    }
    m_FocalPoint += -GetRightDirection() * delta.x * xSpeed * m_Distance;
    m_FocalPoint += GetUpDirection() * delta.y * ySpeed * m_Distance;
}

void Camera::MouseRotate(const glm::vec2& delta)
{
    float yawSign = GetUpDirection().y < 0 ? -1.0f : 1.0f;
    m_Yaw += yawSign * delta.x * RotationSpeed();
    m_Pitch += delta.y * RotationSpeed();
}

void Camera::MouseZoom(float delta)
{
    float zoomSpeed = 4.0f;
    if (IsKeyDown(GLFW_KEY_LEFT_CONTROL))
        zoomSpeed = 20.0f;

    m_FocalPoint -= normalize(GetForwardDirection()) * delta * zoomSpeed;
    // m_Distance -= delta * zoomSpeed * 4;
    // if (m_Distance < 0.01f)
    //{
    //	m_FocalPoint += GetForwardDirection();
    //	m_Distance = 0.01f;
    // }
}

glm::vec3 Camera::CalculatePosition() const
{
    return m_FocalPoint + GetForwardDirection() * m_Distance;
}

std::pair<float, float> Camera::PanSpeed() const
{
    float x       = std::min(m_ViewportWidth / 1000.0f, 2.4f); // max = 2.4f
    float xFactor = 0.0366f * (x * x) - 0.1778f * x + 0.3021f;

    float y       = std::min(m_ViewportHeight / 1000.0f, 2.4f); // max = 2.4f
    float yFactor = 0.0366f * (y * y) - 0.1778f * y + 0.3021f;

    return { xFactor, yFactor };
}

float Camera::RotationSpeed() const
{
    return 0.8f;
}

float Camera::ZoomSpeed() const
{
    float distance = m_Distance * 0.2f;
    distance       = std::max(distance, 0.0f);
    float speed    = distance * distance;
    speed          = std::min(speed, 100.0f); // max speed = 100
    return speed;
}

void Camera::OnMouseScroll(float X, float Y)
{
    float delta = Y * 0.05f;
    MouseZoom(delta);
    UpdateView();
}

bool Camera::IsKeyDown(int keyCode)
{
    return glfwGetKey(
        EngineInternal::GetContext().GetWindow()->GetNativeWindow(),
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
const glm::vec3& Camera::GetUpDirection() const
{
    return glm::rotate(GetOrientation(), glm::vec3(0.0f, 1.0f, 0.0f));
}

const glm::vec3& Camera::GetRightDirection() const
{
    return glm::rotate(GetOrientation(), glm::vec3(1.0f, 0.0f, 0.0f));
}

const glm::vec3& Camera::GetForwardDirection() const
{
    return glm::rotate(GetOrientation(), glm::vec3(0.0f, 0.0f, 1.0f));
}

const glm::quat& Camera::GetOrientation() const
{
    return glm::quat(glm::vec3(-m_Pitch, -m_Yaw, 0.0f));
}
