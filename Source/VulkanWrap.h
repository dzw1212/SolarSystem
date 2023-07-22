#pragma once

#include "vulkan/vulkan.h"
#include "Core.h"

#include <filesystem>
#include "ktx.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"
#include "glm/detail/type_vec3.hpp"
#include "glm/vec4.hpp"
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

struct Vertex3D
{
	glm::vec3 pos = { 0.f, 0.f, 0.f };
	glm::vec3 color = { 0.f, 0.f, 0.f };
	glm::vec2 texCoord = { 0.f, 0.f };
	glm::vec3 normal = { 0.f, 0.f, 0.f };

	static VkVertexInputBindingDescription GetBindingDescription()
	{
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0; //在binding array中的Idx
		bindingDescription.stride = sizeof(Vertex3D);	//所占字节数
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; //传输速率，分为vertex或instance

		return bindingDescription;
	}

	static std::array<VkVertexInputAttributeDescription, 4> GetAttributeDescriptions()
	{
		std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions{};
		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0; //layout (location = 0) in vec3 inPosition;
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Vertex3D, pos);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1; //layout(location = 1) in vec3 inColor;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex3D, color);

		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2; //layout (location = 2) in vec2 inTexCoord;
		attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[2].offset = offsetof(Vertex3D, texCoord);

		attributeDescriptions[3].binding = 0;
		attributeDescriptions[3].location = 3; //layout (location = 3) in vec3 inNormal;
		attributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[3].offset = offsetof(Vertex3D, normal);

		return attributeDescriptions;
	}

	bool operator==(const Vertex3D& other) const
	{
		return (pos == other.pos)
			&& (texCoord == other.texCoord)
			&& (color == other.color)
			&& (normal == other.normal);
	}
};

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
		VkSampler m_Sampler;

		bool IsKtxTexture() { return m_Filepath.extension() == ".ktx"; }

		bool IsTextureArray() { return m_uiLayerNum > 1; }

		bool IsCubemapTexture() { return m_uiFaceNum == 6; }
	};

	class Model
	{
	public:
		struct Primitive 
		{
			UINT uiFirstIndex = 0;
			UINT uiIndexCount = 0;
			int nMaterialIndex = 0;
		};

		struct Mesh 
		{
			std::vector<Primitive> vecPrimitives;
			int nIndex = -1;
		};

		struct Node
		{
			Node* parent = nullptr;
			std::vector<Node*> vecChildren;
			Mesh mesh;
			int nIndex = -1;

			bool HaveMesh()
			{
				return mesh.nIndex >= 0;
			}
		};

		struct Scene
		{
			std::vector<Node> vecNodes;
		};

	public:
		bool IsGLTF() { return m_Filepath.extension() == ".gltf" || m_Filepath.extension() == ".glb"; }
		bool IsOBJ() { return m_Filepath.extension() == ".obj"; }

	public:
		std::vector<Scene> m_vecScenes;

		std::filesystem::path m_Filepath;

		std::vector<Vertex3D> m_vecVertices;
		VkBuffer m_VertexBuffer;
		VkDeviceMemory m_VertexBufferMemory;

		std::vector<UINT> m_vecIndices;
		VkBuffer m_IndexBuffer;
		VkDeviceMemory m_IndexBufferMemory;
	};
}