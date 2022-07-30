#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/glm.hpp>
#include <glm/matrix.hpp>
#include "core.h"
namespace Anor
{
	class Camera
	{
	public:
		Camera() = default;
		Camera(float fov, float aspectRatio, float nearClip, float farClip);

		void OnUpdate(float deltaTime);

		inline float GetDistance() const { return m_Distance; }
		inline void SetDistance(float distance) { m_Distance = distance; }

		inline void SetViewportSize(float width, float height) { m_ViewportWidth = width, m_ViewportHeight = height; UpdateProjection(); }

		glm::vec3 GetUpDirection() const;
		glm::vec3 GetRightDirection() const;
		glm::vec3 GetForwardDirection() const;
		glm::quat GetOrientation() const;
		const glm::vec3& GetPosition() const { return m_Position; }

		float GetPitch() { return m_Pitch; }
		float GetYaw() { return m_Yaw; }

		const glm::mat4& GetProjectionMatrix() { return m_ProjectionMatrix; }
		const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
		const glm::mat4& GetViewProjectionMatrix() const { return m_ViewProjectionMatrix; }
	private:
		void UpdateProjection();
		void UpdateView();

		void MousePan(const glm::vec2& delta);
		void MouseRotate(const glm::vec2& delta);
		void MouseZoom(float delta);

		void OnMouseScroll(float X, float Y);
		

		glm::vec3 CalculatePosition() const;

		std::pair<float, float> PanSpeed() const;
		float RotationSpeed() const;
		float ZoomSpeed() const;
	private:
		bool   IsKeyDown(int keyCode);
		double GetMouseXOffset();
		double GetMouseYOffset();
		bool IsMouseButtonDown(int keyCode);
	private:
		float m_DeltaTime = 0.0f;	// Time between current frame and last frame
		float m_LastFrame = 0.0f; // Time of last frame

		glm::mat4 m_ProjectionMatrix{1.0f};
		glm::mat4 m_ViewMatrix{1.0f};
		glm::mat4 m_ViewProjectionMatrix{ 1.0f };

		float m_FOV = 45.0f;
		float m_AspectRatio = 1.778f;
		float m_NearClip = 0.1f;
		float m_FarClip = 1000.0f;

		glm::vec3 m_FocalPoint = { 0.0f, 0.0f, 0.0f };
		glm::vec3 m_Position = { 0.0f, 0.0f, 0.0f };

		float m_Pitch = 0.0f;
		float m_Yaw = 0.0f;

		glm::vec2 m_InitialMousePosition = { 0.0f, 0.0f };

		float m_Distance = 4.0f;
		float m_ViewportWidth = 1280;
		float m_ViewportHeight = 720;
	};
}