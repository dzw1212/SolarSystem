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
		size_t m_Size = 0;
		UINT m_uiWidth = 0;
		UINT m_uiHeight = 0;
		UINT m_uiMipLevelNum = 0;
		UINT m_uiLayerNum = 0;
		UINT m_uiFaceNum = 0;
		VkImage m_Image;
		VkImageView m_ImageView;
		VkDeviceMemory m_Memory;

		bool IsKtxTexture() { return m_Filepath.extension() == ".ktx"; }

		bool IsTextureArray() { return m_uiLayerNum > 1; }
	};

	class Model
	{
		struct Primitive 
		{
			UINT uiFirstIndex;
			UINT uiIndexCount;
			int nMaterialIndex;
		};

		struct Mesh 
		{
			std::vector<Primitive> vecPrimitives;
		};

		struct Node
		{
			Node* parent = nullptr;
			std::vector<Node*> vecChildren;
			Mesh mesh;
		};
	};
}