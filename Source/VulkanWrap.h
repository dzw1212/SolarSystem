#pragma once

#include "vulkan/vulkan.h"
#include "Core.h"

#include <filesystem>
#include "ktx.h"

namespace DZW_VulkanWrap
{
	struct Texture
	{
		std::filesystem::path m_Filepath;
		size_t m_Size;
		UINT m_uiWidth;
		UINT m_uiHeight;
		UINT m_uiMipLevelNum;
		UINT m_uiLayerNum;
		UINT m_uiFaceNum;
		VkImage m_Image;
		VkImageView m_ImageView;
		VkDeviceMemory m_Memory;



		bool IsKtxTexture() { return m_Filepath.extension() == ".ktx"; }

		bool IsTextureArray() { return m_uiLayerNum > 1; }
	};
}