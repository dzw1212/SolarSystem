#pragma once

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "imgui.h"

#include "../Core.h"

class VulkanRenderer;

class UI
{
public:
	UI() = default;

	void Init(VulkanRenderer* pRenderer);
	void StartNewFrame();
	void Draw();

	void Render(UINT uiIdx);
	void Resize();

	void Clean();
private:
	VulkanRenderer* m_pRenderer;

	VkDescriptorPool m_UIDescriptorPool;
	VkDescriptorSetLayout m_UIDescriptorSetLayout;

	VkSurfaceFormatKHR m_UISurfaceFormat;
	VkPresentModeKHR m_UIPresentMode;
	VkRenderPass m_UIRenderPass;

	VkShaderModule m_UIVertexShaderModule;
	VkShaderModule m_UIVertexSRGBShaderModule;
	VkShaderModule m_UIFragmentShaderModule;

	VkPipelineLayout m_UIPipelineLayout;
	VkPipeline m_UIPipeline;

	//Font
	VkSampler m_UIFontSampler;
	VkImage m_UIFontImage;
	VkDeviceMemory m_UIFontImageMemory;
	VkImageView m_UIFontImageView;
	VkDescriptorSet m_UIFontDescriptorSet;


	VkDeviceMemory      m_UIVertexBufferMemory;
	VkDeviceMemory      m_UIIndexBufferMemory;
	VkDeviceSize        m_UIVertexBufferSize;
	VkDeviceSize        m_UIIndexBufferSize;
	VkBuffer            m_UIVertexBuffer;
	VkBuffer            m_UIIndexBuffer;


private:
	void CreateUIDescriptorPool();
	void CreateUIDescriptorSetLayout();

	void CreateUIPipelineLayout();
	void CreateUIPipeline();

	void CreateUIShaderModule();

	void CreateUIFontSampler();
	void CreateUIFontImageAndBindMemory();
	void CreateUIFontImageView();
	void CreateUIFontDescriptorSet();

	void CreateOrResizeBuffer(VkBuffer& buffer, VkDeviceMemory& buffer_memory, VkDeviceSize& p_buffer_size, size_t new_size, VkBufferUsageFlagBits usage);
	void SetupRenderState(ImDrawData* draw_data, VkPipeline pipeline, VkCommandBuffer command_buffer, int fb_width, int fb_height);

	void UploadFont();
};