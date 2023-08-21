#include "Camera.h"
#include "Core.h"

#include "imgui.h"

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/quaternion.hpp"

void Camera::Init(float fFov, float fWidth, float fHeight, float fNearClip, float fFarClip,
	GLFWwindow* pWindow, const glm::vec3& position, const glm::vec3& focalPoint, bool bFlipY)
{
	ASSERT(fHeight > 0.f);

	m_fVerticalFOV = fFov;
	m_fAspectRatio = fWidth / fHeight;
	m_fViewportWidth = fWidth;
	m_fViewportHeight = fHeight;
	m_fNearClip = fNearClip;
	m_fFarClip = fFarClip;
	m_pWindow = pWindow;
	m_Position = position;
	m_FocalPoint = focalPoint;
	m_bFlipY = bFlipY;


	CalcYawPitch();
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

	bool bPositionChange = false;
	static float fLastTime = glfwGetTime();
	float fCurrentTime = glfwGetTime();
	float fDeltaTime = fCurrentTime - fLastTime;
	fLastTime = fCurrentTime;

	float fSpeed = 10.f;

	if (IsKeyPressed(GLFW_KEY_W))
	{
		m_Position += fSpeed * fDeltaTime * GetForwardDirection();
		m_FocalPoint += fSpeed * fDeltaTime * GetForwardDirection();
		bPositionChange = true;
	}
	else if (IsKeyPressed(GLFW_KEY_S))
	{
		m_Position -= fSpeed * fDeltaTime * GetForwardDirection();
		m_FocalPoint -= fSpeed * fDeltaTime * GetForwardDirection();
		bPositionChange = true;
	}
	else if (IsKeyPressed(GLFW_KEY_A))
	{
		m_Position -= fSpeed * fDeltaTime * GetRightDirection();
		m_FocalPoint -= fSpeed * fDeltaTime * GetRightDirection();
		bPositionChange = true;
	}
	else if (IsKeyPressed(GLFW_KEY_D))
	{
		m_Position += fSpeed * fDeltaTime * GetRightDirection();
		m_FocalPoint += fSpeed * fDeltaTime * GetRightDirection();
		bPositionChange = true;
	}
	else if (IsKeyPressed(GLFW_KEY_Q))
	{
		m_Position += fSpeed * fDeltaTime * GetUpDirection();
		m_FocalPoint += fSpeed * fDeltaTime * GetUpDirection();
		bPositionChange = true;
	}
	else if (IsKeyPressed(GLFW_KEY_E))
	{
		m_Position -= fSpeed * fDeltaTime * GetUpDirection();
		m_FocalPoint -= fSpeed * fDeltaTime * GetUpDirection();
		bPositionChange = true;
	}

	if (bPositionChange)
		UpdateView();

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
	glm::quat qPitch = glm::angleAxis(glm::radians(m_fPitch), glm::vec3(1.0f, 0.0f, 0.0f));
	glm::quat qYaw = glm::angleAxis(glm::radians(m_fYaw), glm::vec3(0.0f, 1.0f, 0.0f));
	glm::quat qRoll = glm::angleAxis(glm::radians(m_fRoll), glm::vec3(0.0f, 0.0f, 1.0f));
	return qYaw * qPitch * qRoll;
}
glm::mat4 Camera::GetRotationMatrix() const
{
	return glm::toMat4(GetRotationQuat());
}
glm::vec3 Camera::GetUpDirection() const
{
	return glm::rotate(GetRotationQuat(), world_y_axis) * (m_bFlipY ? -1.f : 1.f);
}
glm::vec3 Camera::GetRightDirection() const
{
	return glm::rotate(GetRotationQuat(), world_x_axis) * (m_bFlipY ? -1.f : 1.f);
}
glm::vec3 Camera::GetForwardDirection() const
{
	return glm::normalize(m_FocalPoint - m_Position);
}

void Camera::CalcYawPitch()
{
	glm::vec3 forward = glm::normalize(m_Position - m_FocalPoint);

	// 计算yaw和pitch, roll通常设为0
	float yaw = atan2(forward.x, forward.z);
	float pitch = asin(-1.f * forward.y);

	// 转换为角度
	m_fYaw = glm::degrees(yaw);
    m_fPitch = glm::degrees(pitch);
}

void Camera::UpdateView()
{
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
	m_fYaw -= delta.x * 90.f;
	m_fPitch += delta.y * 90.f;

	m_fPitch = std::clamp(m_fPitch, -89.f, 89.f);

	float fCameraDistance = glm::distance(m_Position, m_FocalPoint);

	m_Position = m_FocalPoint + GetRotationQuat() * glm::vec3(0.f, 0.f, fCameraDistance);

	UpdateView();
}
void Camera::CameraZoom(float fDelta)
{
	m_fVerticalFOV -= fDelta * 3.f;
	m_fVerticalFOV = std::clamp(m_fVerticalFOV, 1.f, 135.f);
	UpdateProjection();
}