#pragma once

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "../Core.h"

class VulkanRenderer;

class UI
{
public:
	UI() = default;

	void Init(VulkanRenderer* pRenderer);
	void StartNewFrame();
	void Draw();
	VkCommandBuffer& FillCommandBuffer(UINT uiIdx);

	void RecordRenderPass(UINT uiIdx);

	void Clean();
	void Resize();

private:
	VulkanRenderer* m_pRenderer;
};