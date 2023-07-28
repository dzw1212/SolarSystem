#include "Camera.h"
#include "Core.h"

#include "imgui.h"

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/quaternion.hpp"

Camera::Camera(float fFov, float fAspectRatio, float fNearClip, float fFarClip)
	: m_fVerticalFOV(fFov), m_fAspectRatio(fAspectRatio), m_fNearClip(fNearClip), m_fFarClip(fFarClip)
{
	UpdateProjection();
	UpdateView();
}
void Camera::Set(float fFov, float fAspectRatio, float fNearClip, float fFarClip)
{
	m_fVerticalFOV = fFov;
	m_fAspectRatio = fAspectRatio;
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

	if (IsMouseButtonPressed(GLFW_MOUSE_BUTTON_3))
		MousePan(delta);
	else if (IsMouseButtonPressed(GLFW_MOUSE_BUTTON_1))
		MouseRotate(delta);
	else if (IsMouseButtonPressed(GLFW_MOUSE_BUTTON_2))
		MouseZoom(delta.y);

	UpdateView();
}
void Camera::OnMouseScroll(double offsetX, double offsetY)
{
	float fDelta = offsetY * 0.3f;
	MouseZoom(fDelta);
	UpdateView();
}

void Camera::SetViewportSize(float fWidth, float fHeight)
{
	m_fViewportWidth = fWidth;
	m_fViewportHeight = fHeight;
	UpdateProjection();
}
glm::vec3 Camera::GetUpDirection() const
{
	return glm::rotate(GetOrientation(), y_axis);
}
glm::vec3 Camera::GetRightDirection() const
{
	return glm::rotate(GetOrientation(), x_axis);
}
glm::vec3 Camera::GetForwardDirection() const
{
	return glm::rotate(GetOrientation(), -1.f * z_axis);
}
glm::quat Camera::GetOrientation() const
{
	return glm::quat(-1.f * glm::vec3(m_fPitch, m_fYaw, m_fRoll));
}
void Camera::UpdateView()
{
	m_Position = CalculatePosition();

	auto transform = glm::translate(glm::mat4(1.f), m_Position) * glm::toMat4(GetOrientation());
	m_ViewMatrix = glm::inverse(transform);
}
void Camera::UpdateProjection()
{
	ASSERT(m_fViewportHeight != 0.f, "Viewport Height cant be 0!");
	m_fAspectRatio = m_fViewportWidth / m_fViewportHeight;
	m_ProjMatrix = glm::perspective(glm::radians(m_fVerticalFOV), m_fAspectRatio, m_fNearClip, m_fFarClip);
}

void Camera::MousePan(const glm::vec2& delta)
{
	auto [xSpeed, ySpeed] = PanSpeed();
	m_FocalPoint += -GetRightDirection() * delta.x * xSpeed * m_fDistance;
	m_FocalPoint += GetUpDirection() * delta.y * ySpeed * m_fDistance;
}
void Camera::MouseRotate(const glm::vec2& delta)
{
	float fYawSign = GetUpDirection().y < 0 ? -1.f : 1.f;
	m_fYaw += fYawSign * delta.x * RotateSpeed();
	m_fPitch += delta.y * RotateSpeed();
}
void Camera::MouseZoom(float fDelta)
{
	m_fDistance -= fDelta * ZoomSpeed();
	if (m_fDistance < 1.f)
	{
		m_FocalPoint += GetForwardDirection();
		m_fDistance = 1.f;
	}
}
glm::vec3 Camera::CalculatePosition() const
{
	return m_FocalPoint - GetForwardDirection() * m_fDistance;
}
std::pair<float, float> Camera::PanSpeed() const
{
	float x = std::min(m_fViewportWidth / 1000.f, 2.4f);
	float xFactor = 0.00366f * (x * x) - 0.1778f * x + 0.3021f;

	float y = std::min(m_fViewportHeight / 1000.f, 2.4f);
	float yFactor = 0.00366f * (y * y) - 0.1778f * y + 0.3021f;

	return std::make_pair(xFactor, yFactor);
}
float Camera::RotateSpeed() const
{
	return 0.8f;
}
float Camera::ZoomSpeed() const
{
	float fDistance = std::max(m_fDistance * 0.2f, 0.f);
	float fSpeed = std::min(fDistance * fDistance, 100.f);
	return fSpeed;
}