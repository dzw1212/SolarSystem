#pragma once

#include "vulkan/vulkan.h"
#include "Core.h"

#include <filesystem>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"
#include "glm/detail/type_vec3.hpp"
#include "glm/vec4.hpp"
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

#include "ktx.h"
#include "ktxvulkan.h"


class VulkanRenderer;

namespace tinygltf
{
	class Model;
}

struct Vertex3D
{
	glm::vec3 pos = { 0.f, 0.f, 0.f };
	glm::vec3 color = { 1.f, 1.f, 1.f };
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

namespace DZW_MaterialWrap
{
	struct BlinnPhongMaterial
	{
		glm::vec4 ambientCoefficient = glm::vec4(1.f);
		glm::vec4 diffuseCoefficient = glm::vec4(1.f);
		glm::vec4 specularCoefficient = glm::vec4(1.f);
		float fShininess = 1.f;
	};

	struct PBRMaterial
	{
		glm::vec3 baseColor = glm::vec3(1.f, 0.f, 0.f);
		float fMetallic = 0.f;
		float fRoughness = 0.f;
		float fAO = 0.f;
	};
};

namespace DZW_LightWrap
{
	struct BlinnPhongPointLight
	{
		glm::vec3 position = glm::vec3(0.f);
		float fIntensify = 1.f;

		glm::vec4 color = glm::vec4(1.f);

		//attenuation
		//1.0 / (const + linear * d + quad * d^2)
		float fConstantAttenuation = 1.f;
		float fLinearAttenuation = 0.f;
		float fQuadraticAttenuation = 0.f;
	};

	struct PBRPointLight
	{
		glm::vec3 position = glm::vec3(0.f);
		float fIntensify = 1.f;

		glm::vec4 color = glm::vec4(1.f);

		//attenuation
		//1.0 / (const + linear * d + quad * d^2)
		float fConstantAttenuation = 1.f;
		float fLinearAttenuation = 0.f;
		float fQuadraticAttenuation = 0.f;
	};
}

namespace DZW_MathWrap
{
	struct Circle
	{
		glm::vec3 m_Center = { 0.f, 0.f, 0.f };
		float m_fRadius = 1.f;
	};

	struct Ellipse
	{
		Ellipse(const glm::vec3& center, float fMajor, float fMinor, float fDegSpan)
			: m_Center(center), m_fmajorSemiaxis(fMajor), m_fMinorSemiaxis(fMinor), m_fDegreeSpan(fDegSpan)
		{
			UINT uiIdx = 0;
			for (float fTheta = 0.f; fTheta < 360.f; fTheta += fDegSpan)
			{
				float x = m_fmajorSemiaxis * glm::cos(glm::radians(fTheta));
				float y = m_fMinorSemiaxis * glm::sin(glm::radians(fTheta));
				Vertex3D vert;
				vert.pos = glm::vec3(x, y, 0.f) + m_Center;
				vert.color = { 1.f, 0.f, 0.f };
				m_vecVertices.push_back(vert);
				m_vecIndices.push_back(uiIdx++);
			}
			m_vecIndices.push_back(0);
		}

		glm::vec3 m_Center = { 0.f, 0.f, 0.f };
		float m_fmajorSemiaxis = 1.f;
		float m_fMinorSemiaxis = 0.5f;

		float m_fDegreeSpan = 1.f;
		std::vector<Vertex3D> m_vecVertices;
		std::vector<UINT> m_vecIndices;
	};
}

namespace DZW_VulkanWrap
{
	class Texture
	{
	public:
		Texture(VulkanRenderer* pRenderer, const std::filesystem::path& filepath)
			: m_pRenderer(pRenderer), m_Filepath(filepath) {}
		~Texture();

		enum class TextureType : UCHAR
		{
			TEXTURE_TYPE_KTX,
			TEXTURE_TYPE_NORMAL,
		};

		virtual TextureType GetType() = 0;

		bool IsTextureArray() { return m_uiLayerNum > 1; }
		bool IsCubemapTexture() { return m_uiFaceNum == 6; }

		void CreateImage();
		void CreateImageView();
		void CreateSampler();

	public:
		std::filesystem::path m_Filepath;
		VulkanRenderer* m_pRenderer = nullptr;

		size_t m_Size = 0;
		UINT m_uiWidth = 0;
		UINT m_uiHeight = 0;
		UINT m_uiMipLevelNum = 0;
		UINT m_uiLayerNum = 0;
		UINT m_uiFaceNum = 0;
	public:
		//vulkan resource
		VkImage m_Image = VK_NULL_HANDLE;
		VkImageView m_ImageView = VK_NULL_HANDLE;
		VkDeviceMemory m_Memory = VK_NULL_HANDLE;
		VkSampler m_Sampler = VK_NULL_HANDLE;
	};

	class NormalTexture : public Texture
	{
	public:
		NormalTexture(VulkanRenderer* pRenderer, const std::filesystem::path& filepath);

		virtual TextureType GetType() { return TextureType::TEXTURE_TYPE_KTX; }
	};

	class KTXTexture : public Texture
	{
	public:
		KTXTexture(VulkanRenderer* pRenderer, const std::filesystem::path& filepath);

		virtual TextureType GetType() { return TextureType::TEXTURE_TYPE_KTX; }
	
	private:
		void TransferImageDataByStageBuffer(const void* pData, VkDeviceSize imageSize, VkImage& image, UINT uiWidth, UINT uiHeight, ktxTexture* pKtxTextue);
	};

	class TextureFactor
	{
	public:
		static std::unique_ptr<Texture> CreateTexture(VulkanRenderer* pRenderer, const std::filesystem::path& filepath);
	};


	class Model
	{
	public:
		Model(VulkanRenderer* pRenderer, const std::filesystem::path& filepath)
			: m_pRenderer(pRenderer), m_Filepath(filepath) {}
		virtual ~Model() {}

		enum class ModelType : UCHAR
		{
			MODEL_TYPE_OBJ,
			MODEL_TYPE_GLTF,
		};

		virtual ModelType GetType() = 0;

		virtual void Draw(VkCommandBuffer& commandBuffer, VkPipeline& pipeline, VkPipelineLayout& pipelineLayout, VkDescriptorSet* pDescriptorSet = nullptr) = 0;
	public:
		VulkanRenderer* m_pRenderer = nullptr;
		std::filesystem::path m_Filepath;

		std::vector<Vertex3D> m_vecVertices;
		VkBuffer m_VertexBuffer = VK_NULL_HANDLE;
		VkDeviceMemory m_VertexBufferMemory = VK_NULL_HANDLE;
		std::vector<UINT> m_vecIndices;
		VkBuffer m_IndexBuffer = VK_NULL_HANDLE;
		VkDeviceMemory m_IndexBufferMemory = VK_NULL_HANDLE;
	};

	class OBJModel : public Model
	{
	public:
		OBJModel(VulkanRenderer* pRenderer, const std::filesystem::path& filepath);
		virtual ~OBJModel();

		virtual ModelType GetType() { return ModelType::MODEL_TYPE_OBJ; }

		virtual void Draw(VkCommandBuffer& commandBuffer, VkPipeline& pipeline, VkPipelineLayout& pipelineLayout, VkDescriptorSet* pDescriptorSet = nullptr);
	
	private: 
		VkDescriptorSet m_DescriptorSet;
	};

	class GLTFModel : public Model
	{
	public:
		struct Primitive 
		{
			UINT m_uiFirstIndex = 0;
			UINT m_uiIndexCount = 0;
			int m_nMaterialIdx = 0;

			VkDescriptorSet m_DescriptorSet = VK_NULL_HANDLE;
		};

		struct Mesh 
		{
			std::string strName;
			std::vector<Primitive> vecPrimitives;
		};

		struct Node
		{
			int m_ParentIdx = -1;
			std::vector<int> m_vecChildren;

			std::string strName;
			int m_nMeshIdx;
			int m_nIdx;

			glm::mat4 modelMatrix;
		};

		struct Scene
		{
			std::vector<int> m_vecHeadNodes;
		};

		struct Image
		{
			std::string m_strName;
			UINT m_uiWidth;
			UINT m_uiHeight;
			VkImage m_Image;
			VkImageView m_ImageView;
			VkDeviceMemory m_Memory;
		};

		struct Sampler
		{
			VkSampler m_Sampler;
		};

		struct Texture
		{
			int m_nImageIdx;
			int m_nSamplerIdx;
		};

		struct Material
		{
			//glTF中，通常将occlusion/metallic/roughness放在同一个texture中
			//R/G/B三个通道分别对应occlusion/metallic/roughness
			std::string m_strName;
			glm::vec4 m_BaseColorFactor = glm::vec4(1.f);
			int m_nBaseColotTextureIdx;

			float m_fMetallicFactor = 1.f;
			float m_fRoughnessFactor = 1.f;
			int m_nMetallicRoughnessTextureIdx = -1;

			float m_fNormalScale = 1.f;
			int m_nNormalTextureIdx = -1;
			
			float m_fOcclusionStrength = 1.f;
			int m_nOcclusionTextureIdx = -1;

			glm::vec3 m_EmmisiveFactor = glm::vec3(1.f);
			int m_nEmmisiveTextureIdx = -1;
		};

	public:
		GLTFModel(VulkanRenderer* pRenderer, const std::filesystem::path& filepath);
		virtual ~GLTFModel();

		virtual ModelType GetType() { return ModelType::MODEL_TYPE_GLTF; }

		virtual void Draw(VkCommandBuffer& commandBuffer, VkPipeline& pipeline, VkPipelineLayout& pipelineLayout, VkDescriptorSet* pDescriptorSet = nullptr);

		void DrawNode(int nNodeIdx, VkCommandBuffer& commandBuffer, VkPipeline& pipeline, VkPipelineLayout& pipelineLayout);
	private:
		void LoadImages(const tinygltf::Model& gltfModel);
		void LoadSamplers(const tinygltf::Model& gltfModel);
		void LoadTextures(const tinygltf::Model& gltfModel);
		void LoadMaterials(const tinygltf::Model& gltfModel);
		void LoadNodes(const tinygltf::Model& gltfModel);
		void LoadMeshes(const tinygltf::Model& gltfModel);

		void LoadNodeRelation(Node* parentNode, int nNodeIdx);
	private:
		Scene m_DefaultScene; //目前仅支持载入默认场景

		std::vector<Image> m_vecImages;
		std::vector<Sampler> m_vecSamplers;
		std::vector<Texture> m_vecTextures;
		std::vector<Material> m_vecMaterials;
		std::vector<Node> m_vecNodes;
		std::vector<Mesh> m_vecMeshes;
	};

	class ModelFactor
	{
	public:
		static std::unique_ptr<Model> CreateModel(VulkanRenderer* pRenderer, const std::filesystem::path& filepath);
	};
}