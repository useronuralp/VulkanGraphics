#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include "core.h"

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/matrix.hpp>
class Camera
{
   public:
    Camera() = default;
    Camera(float fov, float aspectRatio);

    void OnUpdate(float deltaTime);

    void SetDistance(float distance);
    void SetViewportSize(float width, float height);

    float     GetDistance() const;
    glm::vec3 GetPosition() const;
    float     GetPitch() const;
    float     GetYaw() const;
    glm::mat4 GetProjectionMatrix() const;
    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetViewProjectionMatrix() const;
    float     GetNearClip() const;
    float     GetFarClip() const;
    glm::vec3 GetUpDirection() const;
    glm::vec3 GetRightDirection() const;
    glm::vec3 GetForwardDirection() const;
    glm::quat GetOrientation() const;

   private:
    void                    UpdateProjection();
    void                    UpdateView();
    void                    MousePan(const glm::vec2& delta, float deltaTime);
    void                    MouseRotate(const glm::vec2& delta, float deltaTime);
    void                    MouseZoom(float delta, float deltaTime);
    void                    OnMouseScroll(float X, float Y, float deltaTime);
    glm::vec3               CalculatePosition() const;
    std::pair<float, float> PanSpeed() const;
    float                   RotationSpeed() const;
    float                   ZoomSpeed() const;

    bool   IsKeyDown(int keyCode);
    double GetMouseXOffset();
    double GetMouseYOffset();
    bool   IsMouseButtonDown(int keyCode);

    glm::mat4 _ProjectionMatrix{ 1.0f };
    glm::mat4 _ViewMatrix{ 1.0f };
    glm::mat4 _ViewProjectionMatrix{ 1.0f };
    float     _FOV                  = 45.0f;
    float     _AspectRatio          = 1.778f;
    float     _NearClip             = 0.1f;
    float     _FarClip              = 1500.0f;
    glm::vec3 _FocalPoint           = { 0.0f, 0.0f, 0.0f };
    glm::vec3 _Position             = { 0.0f, 0.0f, 0.0f };
    float     _Pitch                = 0.0f;
    float     _Yaw                  = 0.0f;
    glm::vec2 _InitialMousePosition = { 0.0f, 0.0f };
    float     _Distance             = 4.0f;
    float     _ViewportWidth        = 1280;
    float     _ViewportHeight       = 720;
};
