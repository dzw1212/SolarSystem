#include "Camera.h"
#include "Core.h"

#include "imgui.h"

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/quaternion.hpp"

Camera::Camera(float fFov, float fAspectRatio, float fNearClip, float fFarClip)
	: m_fVerticalFOV(fFov), m_fAspectRatio(fAspectRatio), m_fNearClip(fNearClip), m_fFarClip(fFarClip)
{
}
void Camera::Init(float fFov, float fWidth, float fHeight, float fNearClip, float fFarClip)
{
	ASSERT(fHeight > 0.f);

	m_fVerticalFOV = fFov;
	m_fAspectRatio = fWidth / fHeight;
	m_fViewportWidth = fWidth;
	m_fViewportHeight = fHeight;
	m_fNearClip = fNearClip;
	m_fFarClip = fFarClip;

	UpdateProjection();
	UpdateView();
}
bool Camera::IsKeyPressed(int nKeycode)
{
	return glfwGetKey(m_pWindow, nKeycode) == GLFW_PRESS;
}
bool Camera::IsMouseButtonPressed(int nMouseButton)
{
	return glfwGetMouseButton(m_pWindow, nMouseButton) == GLFW_PRESS;
}
glm::vec2 Camera::GetMousePos()
{
	double posx, posy;
	glfwGetCursorPos(m_pWindow, &posx, &posy);
	return glm::vec2(posx, posy);
}
void Camera::Tick()
{
	const glm::vec2& curMousePos = GetMousePos();
	glm::vec2 delta = (curMousePos - m_InititalMousePosition) * 0.003f;
	m_InititalMousePosition = curMousePos;

	if (IsMouseButtonPressed(GLFW_MOUSE_BUTTON_1))
		CameraRotate(delta);
	else if (IsMouseButtonPressed(GLFW_MOUSE_BUTTON_2))
		CameraZoom(-5.f * delta.y);

	//if (IsKeyPressed(GLFW_KEY_W))
	//	MouseZoom(2.f);
	//else if (IsKeyPressed(GLFW_KEY_A))
	//	m_FocalPoint -= GetRightDirection() * 2.f;
	//else if (IsKeyPressed(GLFW_KEY_D))
	//	m_FocalPoint += GetRightDirection() * 2.f;
	//else if (IsKeyPressed(GLFW_KEY_S))
	//	MouseZoom(-2.f);
}
void Camera::OnMouseScroll(double offsetX, double offsetY)
{
	float fDelta = offsetY * 0.3f;
	CameraZoom(fDelta);
	UpdateView();
}

void Camera::OnKeyPress(int nKey)
{

}

void Camera::SetViewportSize(float fWidth, float fHeight)
{
	m_fViewportWidth = fWidth;
	m_fViewportHeight = fHeight;
	UpdateProjection();
}
glm::quat Camera::GetRotationQuat() const
{
	return glm::quat(glm::vec3(glm::radians(m_fYaw), glm::radians(m_fPitch), glm::radians(m_fRoll)));
}
glm::mat4 Camera::GetRotationMatrix() const
{
	return glm::toMat4(GetRotationQuat());
}
glm::vec3 Camera::GetUpDirection() const
{
	return glm::rotate(GetRotationQuat(), y_axis);
}
glm::vec3 Camera::GetRightDirection() const
{
	return glm::rotate(GetRotationQuat(), x_axis);
}
glm::vec3 Camera::GetForwardDirection() const
{
	return glm::rotate(GetRotationQuat(), -1.f * z_axis);
}

void Camera::UpdateView()
{
	//auto transform = glm::translate(glm::mat4(1.f), m_Position) * glm::toMat4(GetRotationQuat());
	//m_ViewMatrix = glm::inverse(transform);

	m_ViewMatrix = glm::lookAt(m_Position, m_FocalPoint, GetUpDirection());
}
void Camera::UpdateProjection()
{
	ASSERT(m_fViewportHeight != 0.f, "Viewport Height cant be 0!");
	m_fAspectRatio = m_fViewportWidth / m_fViewportHeight;
	m_ProjMatrix = glm::perspective(glm::radians(m_fVerticalFOV), m_fAspectRatio, m_fNearClip, m_fFarClip);
	if (m_bFlipY)
		m_ProjMatrix[1][1] *= -1.f;
}

void Camera::CameraRotate(const glm::vec2& delta)
{
	m_fYaw += delta.y * 90.f;
	m_fPitch -= delta.x * 90.f;

	float fCameraDistance = glm::distance(m_Position, m_FocalPoint);

	m_Position = m_FocalPoint + GetRotationQuat() * glm::vec3(0.f, 0.f, fCameraDistance);

	UpdateView();
}
void Camera::CameraZoom(float fDelta)
{
	m_fVerticalFOV -= fDelta * 3.f;
	UpdateProjection();
}