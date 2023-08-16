#pragma once

#include "GLFW/glfw3.h"
#include "glm/glm.hpp"

class Camera
{
public:
	Camera() = default;
	Camera(const Camera& camera) = default;
	Camera(float fFov, float fAspectRatio, float fNearClip, float fFarClip);

	void Init(float fFov, float fWidth, float fHeight, float fNearClip, float fFarClip);

	bool IsKeyPressed(int nKeycode);
	bool IsMouseButtonPressed(int nMouseButton);
	glm::vec2 GetMousePos();

	void Tick();
	void OnMouseScroll(double offsetX, double offsetY);
	void OnKeyPress(int nKey);

	inline void SetWindow(GLFWwindow* pWindow) { m_pWindow = pWindow; }

	void SetViewportSize(float fWidth, float fHeight);

	const glm::mat4& GetProjMatrix() const { return m_ProjMatrix; }
	const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
	glm::mat4 GetViewProjMatrix() const { return m_ProjMatrix * m_ViewMatrix; }

	glm::quat GetRotationQuat() const;
	glm::mat4 GetRotationMatrix() const;

	glm::vec3 GetUpDirection() const;
	glm::vec3 GetRightDirection() const;
	glm::vec3 GetForwardDirection() const;

	const glm::vec3& GetPosition() { return m_Position; }
	void SetPosition(const glm::vec3& pos) { m_Position = pos; }

	const glm::vec3& GetFocalPoint() { return m_FocalPoint; }
	void SetFocalPoint(const glm::vec3& pos) { m_FocalPoint = pos; }

	void SetFlipY(bool bFlipY) { m_bFlipY = bFlipY; }

	float GetVerticalFOV() { return m_fVerticalFOV; }
	float GetAspectRatio() { return m_fAspectRatio; }

	float GetPitch() const { return m_fPitch; }
	float GetYaw() const { return m_fYaw; }
	float GetRoll() const { return m_fRoll; }


	void UpdateView();
	void UpdateProjection();

private:
	void CameraRotate(const glm::vec2& delta);
	void CameraZoom(float fDelta);

private:
	const glm::vec3 x_axis = { 1.f, 0.f, 0.f };
	const glm::vec3 y_axis = { 0.f, 1.f, 0.f };
	const glm::vec3 z_axis = { 0.f, 0.f, 1.f };

private:
	float m_fVerticalFOV = 45.f;
	float m_fAspectRatio = 1.f;
	float m_fNearClip = 0.1f;
	float m_fFarClip = 1000.f;

	glm::mat4 m_ViewMatrix;
	glm::mat4 m_ProjMatrix;

	glm::vec3 m_Position = { 0.f, 0.f, 5.f };
	glm::vec3 m_FocalPoint = { 0.f, 0.f, 0.f };

	float m_fYaw = 0.f;
	float m_fPitch = 0.f;
	float m_fRoll = 0.f;

	float m_fViewportWidth = 1.f;
	float m_fViewportHeight = 1.f;

	glm::vec2 m_InititalMousePosition;

	bool m_bFlipY = false;

	GLFWwindow* m_pWindow = nullptr;
};
