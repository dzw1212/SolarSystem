#include "VulkanWrap.h"
#include "VulkanRenderer.h"

#include "Log.h"
#include "VulkanUtils.h"

#include <random>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

//#define STB_IMAGE_IMPLEMENTATION
//#define STB_IMAGE_WRITE_IMPLEMENTATION
//#include "stb_image.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
// #define TINYGLTF_NOEXCEPTION // optional. disable exception handling.
#include "tiny_gltf.h"

namespace DZW_VulkanWrap
{
	std::unique_ptr<Texture> TextureFactor::CreateTexture(VulkanRenderer* pRenderer, const std::filesystem::path& filepath)
	{
		if (filepath.extension() == ".ktx")
			return std::make_unique<KTXTexture>(pRenderer, filepath);
		else if (filepath.extension() == ".jpg"
			|| filepath.extension() == ".png"
			|| filepath.extension() == ".tga"
			|| filepath.extension() == ".bmp"
			|| filepath.extension() == ".gif")
			return std::make_unique<NormalTexture>(pRenderer, filepath);
		else
		{
			Log::Error("Unsupport texture format");
			return nullptr;
		}
	}

	Texture::~Texture()
	{
		vkDestroyImage(m_pRenderer->m_LogicalDevice, m_Image, nullptr);
		vkDestroyImageView(m_pRenderer->m_LogicalDevice, m_ImageView, nullptr);
		vkFreeMemory(m_pRenderer->m_LogicalDevice, m_Memory, nullptr);
		vkDestroySampler(m_pRenderer->m_LogicalDevice, m_Sampler, nullptr);
	}

	void Texture::CreateImage()
	{
		m_pRenderer->CreateImageAndBindMemory(m_uiWidth, m_uiHeight,
			m_uiMipLevelNum, m_uiLayerNum, m_uiFaceNum,
			VK_SAMPLE_COUNT_1_BIT,
			VK_FORMAT_R8G8B8A8_SRGB,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			m_Image, m_Memory);
	}

	void Texture::CreateImageView()
	{
		m_ImageView = m_pRenderer->CreateImageView(m_Image,
			VK_FORMAT_R8G8B8A8_SRGB,
			VK_IMAGE_ASPECT_COLOR_BIT,
			m_uiMipLevelNum,
			m_uiLayerNum,
			m_uiFaceNum
		);
	}

	void Texture::CreateSampler()
	{
		VkSamplerCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		//设置过采样与欠采样时的采样方法，可以是nearest，linear，cubic等
		createInfo.magFilter = VK_FILTER_LINEAR;
		createInfo.minFilter = VK_FILTER_LINEAR;

		//设置纹理采样超出边界时的寻址模式，可以是repeat，mirror，clamp to edge，clamp to border等
		createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

		//设置是否开启各向异性过滤，你的硬件不一定支持Anisotropy，需要确认硬件支持该preperty
		createInfo.anisotropyEnable = VK_FALSE;
		if (createInfo.anisotropyEnable == VK_TRUE)
		{
			auto& physicalDeviceProperties = m_pRenderer->m_mapPhysicalDeviceInfo.at(m_pRenderer->m_PhysicalDevice).properties;
			createInfo.maxAnisotropy = physicalDeviceProperties.limits.maxSamplerAnisotropy;
		}

		//设置寻址模式为clamp to border时的填充颜色
		createInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

		//如果为true，则坐标为[0, texWidth), [0, texHeight)
		//如果为false，则坐标为传统的[0, 1), [0, 1)
		createInfo.unnormalizedCoordinates = VK_FALSE;

		//设置是否开启比较与对比较结果的操作，通常用于百分比邻近滤波（Shadow Map PCS）
		createInfo.compareEnable = VK_FALSE;
		createInfo.compareOp = VK_COMPARE_OP_ALWAYS;

		//设置mipmap相关参数
		createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		createInfo.mipLodBias = 0.f;
		createInfo.minLod = 0.f;
		createInfo.maxLod = static_cast<float>(m_uiMipLevelNum);

		VULKAN_ASSERT(vkCreateSampler(m_pRenderer->m_LogicalDevice, &createInfo, nullptr, &m_Sampler), "Create texture sampler failed");
	}

	NormalTexture::NormalTexture(VulkanRenderer* pRenderer, const std::filesystem::path& filepath)
		: Texture(pRenderer, filepath)
	{
		//stb库是一个轻量级的图像处理库，无法直接读取图片的mipmap层级

		//创建Image，Memory
		int nTexWidth = 0;
		int nTexHeight = 0;
		int nTexChannel = 0;
		stbi_uc* pixels = stbi_load(m_Filepath.string().c_str(), &nTexWidth, &nTexHeight, &nTexChannel, STBI_rgb_alpha);
		ASSERT(pixels, std::format("stb load image {} failed", m_Filepath.string()));

		ASSERT(nTexChannel == 4);

		m_uiWidth = static_cast<UINT>(nTexWidth);
		m_uiHeight = static_cast<UINT>(nTexHeight);
		m_uiMipLevelNum = 1;
		m_uiLayerNum = 1;
		m_uiFaceNum = 1;

		m_Size = m_uiWidth * m_uiHeight * static_cast<UINT>(nTexChannel);

		CreateImage();

		//copy之前，将layout从初始的undefined转为transfer dst
		m_pRenderer->ChangeImageLayout(m_Image,
			VK_FORMAT_R8G8B8A8_SRGB,
			m_uiMipLevelNum,
			m_uiLayerNum,
			m_uiFaceNum,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		m_pRenderer->TransferImageDataByStageBuffer(pixels, m_Size, m_Image, m_uiWidth, m_uiHeight);

		stbi_image_free(pixels);
		
		//copy之后，将layout转为shader readonly
		m_pRenderer->ChangeImageLayout(m_Image,
			VK_FORMAT_R8G8B8A8_SRGB,
			m_uiMipLevelNum,
			m_uiLayerNum,
			m_uiFaceNum,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		CreateImageView();
		CreateSampler();
	}

	KTXTexture::KTXTexture(VulkanRenderer* pRenderer, const std::filesystem::path& filepath)
		: Texture(pRenderer, filepath)
	{
		ktxResult result;
		ktxTexture* pKtxTexture;
		result = ktxTexture_CreateFromNamedFile(m_Filepath.string().c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &pKtxTexture);
		ASSERT(result == KTX_SUCCESS, "ktx load image {} failed", m_Filepath.string());

		ASSERT(pKtxTexture->glFormat == GL_RGBA);
		ASSERT(pKtxTexture->numDimensions == 2);

		ktx_uint8_t* ktxTextureData = ktxTexture_GetData(pKtxTexture);
		m_Size = ktxTexture_GetSize(pKtxTexture);

		m_uiWidth = pKtxTexture->baseWidth;
		m_uiHeight = pKtxTexture->baseHeight;
		m_uiMipLevelNum = pKtxTexture->numLevels;
		m_uiLayerNum = pKtxTexture->numLayers;
		m_uiFaceNum = pKtxTexture->numFaces;

		if (IsTextureArray())
		{
			auto& physicalDeviceInfo = m_pRenderer->m_mapPhysicalDeviceInfo.at(m_pRenderer->m_PhysicalDevice);
			UINT uiMaxLayerNum = physicalDeviceInfo.properties.limits.maxImageArrayLayers;
			ASSERT(m_uiLayerNum <= uiMaxLayerNum, "TextureArray {} layout count {} exceed max limit {}", m_Filepath.string(), m_uiLayerNum, uiMaxLayerNum);
		}

		CreateImage();

		//copy之前，将layout从初始的undefined转为transfer dst
		m_pRenderer->ChangeImageLayout(m_Image,
			VK_FORMAT_R8G8B8A8_SRGB,
			m_uiMipLevelNum,
			m_uiLayerNum,
			m_uiFaceNum,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		TransferImageDataByStageBuffer(ktxTextureData, m_Size, m_Image, m_uiWidth, m_uiHeight, pKtxTexture);

		ktxTexture_Destroy(pKtxTexture);

		m_pRenderer->ChangeImageLayout(m_Image,
			VK_FORMAT_R8G8B8A8_SRGB,
			m_uiMipLevelNum,
			m_uiLayerNum,
			m_uiFaceNum,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		CreateImageView();
		CreateSampler();
	}

	void KTXTexture::TransferImageDataByStageBuffer(const void* pData, VkDeviceSize imageSize, VkImage& image, UINT uiWidth, UINT uiHeight, ktxTexture* pKtxTexture)
	{
		ASSERT(pKtxTexture != nullptr, "Ktx imgae data is empty");

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;

		m_pRenderer->CreateBufferAndBindMemory(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer, stagingBufferMemory);

		void* imageData;
		vkMapMemory(m_pRenderer->m_LogicalDevice, stagingBufferMemory, 0, imageSize, 0, (void**)&imageData);
		memcpy(imageData, pData, static_cast<size_t>(imageSize));
		vkUnmapMemory(m_pRenderer->m_LogicalDevice, stagingBufferMemory);

		VkCommandBuffer singleTimeCommandBuffer = m_pRenderer->BeginSingleTimeCommand();

		std::vector<VkBufferImageCopy> vecBufferCopyRegions;
		for (UINT face = 0; face < m_uiFaceNum; ++face)
		{
			for (UINT layer = 0; layer < m_uiLayerNum; ++layer)
			{
				for (UINT mipLevel = 0; mipLevel < m_uiMipLevelNum; ++mipLevel)
				{
					size_t offset;
					KTX_error_code ret = ktxTexture_GetImageOffset(pKtxTexture, mipLevel, layer, face, &offset);
					ASSERT(ret == KTX_SUCCESS);
					VkBufferImageCopy bufferCopyRegion = {};
					bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					bufferCopyRegion.imageSubresource.mipLevel = mipLevel;
					bufferCopyRegion.imageSubresource.baseArrayLayer = (m_uiFaceNum > 1) ? (face + layer * 6) : (layer);
					bufferCopyRegion.imageSubresource.layerCount = 1;
					bufferCopyRegion.imageExtent.width = uiWidth >> mipLevel;
					bufferCopyRegion.imageExtent.height = uiHeight >> mipLevel;
					bufferCopyRegion.imageExtent.depth = 1;
					bufferCopyRegion.bufferOffset = offset;
					vecBufferCopyRegions.push_back(bufferCopyRegion);
				}
			}
		}

		vkCmdCopyBufferToImage(singleTimeCommandBuffer, stagingBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			static_cast<UINT>(vecBufferCopyRegions.size()), vecBufferCopyRegions.data());


		m_pRenderer->EndSingleTimeCommand(singleTimeCommandBuffer);

		vkDestroyBuffer(m_pRenderer->m_LogicalDevice, stagingBuffer, nullptr);
		vkFreeMemory(m_pRenderer->m_LogicalDevice, stagingBufferMemory, nullptr);
	}




	std::unique_ptr<Model> ModelFactor::CreateModel(VulkanRenderer* pRenderer, const std::filesystem::path& filepath)
	{
		if (!pRenderer)
			return nullptr;

		if (filepath.extension() == ".obj")
			return std::make_unique<OBJModel>(pRenderer, filepath);
		else if (filepath.extension() == ".gltf" || filepath.extension() == ".glb")
			return std::make_unique<GLTFModel>(pRenderer, filepath);
		else
		{
			Log::Error("Unsupport model format");
			return nullptr;
		}
	}

	OBJModel::OBJModel(VulkanRenderer* pRenderer, const std::filesystem::path& filepath)
		: Model(pRenderer, filepath)
	{
		tinyobj::attrib_t attr;	//存储所有顶点、法线、UV坐标
		std::vector<tinyobj::shape_t> vecShapes;
		std::vector<tinyobj::material_t> vecMaterials;
		std::string strWarning;
		std::string strError;

		bool res = tinyobj::LoadObj(&attr, &vecShapes, &vecMaterials, &strWarning, &strError, m_Filepath.string().c_str());
		ASSERT(res, std::format("Load obj model {} failed", m_Filepath.string().c_str()));

		m_vecVertices.clear();
		m_vecIndices.clear();

		for (size_t s = 0; s < vecShapes.size(); s++)
		{
			size_t index_offset = 0;

			// 遍历所有面
			for (size_t f = 0; f < vecShapes[s].mesh.num_face_vertices.size(); f++)
			{
				int fv = vecShapes[s].mesh.num_face_vertices[f];

				// 遍历所有顶点
				for (size_t v = 0; v < fv; v++)
				{
					tinyobj::index_t idx = vecShapes[s].mesh.indices[index_offset + v];
					Vertex3D vert{};

					vert.pos = {
						attr.vertices[3 * static_cast<UINT64>(idx.vertex_index) + 0],
						attr.vertices[3 * static_cast<UINT64>(idx.vertex_index) + 1],
						attr.vertices[3 * static_cast<UINT64>(idx.vertex_index) + 2],
					};

					vert.texCoord = {
						attr.texcoords[2 * static_cast<UINT64>(idx.texcoord_index) + 0],
						1.f - attr.texcoords[2 * static_cast<UINT64>(idx.texcoord_index) + 1],
					};

					if (attr.normals.size() > 0)
					{
						vert.normal = {
							attr.normals[3 * static_cast<UINT64>(idx.normal_index) + 0],
							attr.normals[3 * static_cast<UINT64>(idx.normal_index) + 1],
							attr.normals[3 * static_cast<UINT64>(idx.normal_index) + 2],
						};
					}
					
					if (false) //random color
					{
						std::random_device rd;
						std::mt19937 gen(rd());
						std::uniform_int_distribution<> dis(0, 255);

						int red = dis(gen);
						int green = dis(gen);
						int blue = dis(gen);

						vert.color = { red / 255.f, green / 255.f, blue / 255.f };
					}
					else
						vert.color = { 1.f, 1.f, 1.f };
					

					if (true) //vulkan flipY
					{
						vert.pos.y *= -1.f;
						vert.normal.y *= -1.f;
					}

					m_vecVertices.push_back(vert);
					m_vecIndices.push_back(static_cast<UINT>(m_vecIndices.size()));
				}

				index_offset += fv;
			}
		}

		ASSERT(m_vecVertices.size() > 0, "Vertex data empty");
		VkDeviceSize verticesSize = sizeof(m_vecVertices[0]) * m_vecVertices.size();
		m_pRenderer->CreateBufferAndBindMemory(verticesSize,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			m_VertexBuffer, m_VertexBufferMemory);
		m_pRenderer->TransferBufferDataByStageBuffer(m_vecVertices.data(), verticesSize, m_VertexBuffer);


		VkDeviceSize indicesSize = sizeof(m_vecIndices[0]) * m_vecIndices.size();
		if (m_vecIndices.size() > 0) //存在只有Vertices而没有Indices的模型
		{
			m_pRenderer->CreateBufferAndBindMemory(indicesSize,
				VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				m_IndexBuffer, m_IndexBufferMemory);
			m_pRenderer->TransferBufferDataByStageBuffer(m_vecIndices.data(), indicesSize, m_IndexBuffer);
		}

		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorSetCount = 1;
		allocInfo.descriptorPool = m_pRenderer->m_CommonDescriptorPool;
		allocInfo.pSetLayouts = &m_pRenderer->m_CommonDescriptorSetLayout;

		VULKAN_ASSERT(vkAllocateDescriptorSets(m_pRenderer->m_LogicalDevice, &allocInfo, &m_DescriptorSet), "Allocate obj desctiprot sets failed");

		//ubo
		VkDescriptorBufferInfo descriptorBufferInfo{};
		descriptorBufferInfo.buffer = m_pRenderer->m_CommonMVPUniformBuffer;
		descriptorBufferInfo.offset = 0;
		descriptorBufferInfo.range = sizeof(VulkanRenderer::CommonMVPUniformBufferObject);

		VkWriteDescriptorSet uboWrite{};
		uboWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		uboWrite.dstSet = m_DescriptorSet;
		uboWrite.dstBinding = 0;
		uboWrite.dstArrayElement = 0;
		uboWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboWrite.descriptorCount = 1;
		uboWrite.pBufferInfo = &descriptorBufferInfo;

		//shadowMap sampler
		VkDescriptorImageInfo shadowMapImageInfo{};
		shadowMapImageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		shadowMapImageInfo.imageView = m_pRenderer->m_ShadowMapDepthImageView;
		shadowMapImageInfo.sampler = m_pRenderer->m_ShadowMapSampler;

		VkWriteDescriptorSet shadowMapSamplerWrite{};
		shadowMapSamplerWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		shadowMapSamplerWrite.dstSet = m_DescriptorSet;
		shadowMapSamplerWrite.dstBinding = 1;
		shadowMapSamplerWrite.dstArrayElement = 0;
		shadowMapSamplerWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		shadowMapSamplerWrite.descriptorCount = 1;
		shadowMapSamplerWrite.pImageInfo = &shadowMapImageInfo;

		std::vector<VkWriteDescriptorSet> vecDescriptorWrite = {
			uboWrite,
			shadowMapSamplerWrite
		};

		vkUpdateDescriptorSets(m_pRenderer->m_LogicalDevice, static_cast<UINT>(vecDescriptorWrite.size()), vecDescriptorWrite.data(), 0, nullptr);
	}

	OBJModel::~OBJModel()
	{
		vkFreeMemory(m_pRenderer->m_LogicalDevice, m_VertexBufferMemory, nullptr);
		vkDestroyBuffer(m_pRenderer->m_LogicalDevice, m_VertexBuffer, nullptr);

		if (m_vecIndices.size() > 0)
		{
			vkFreeMemory(m_pRenderer->m_LogicalDevice, m_IndexBufferMemory, nullptr);
			vkDestroyBuffer(m_pRenderer->m_LogicalDevice, m_IndexBuffer, nullptr);
		}
	}

	void OBJModel::Draw(VkCommandBuffer& commandBuffer, VkPipeline& pipeline, VkPipelineLayout& pipelineLayout, VkDescriptorSet* pDescriptorSet)
	{
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		VkBuffer VertexBuffers[] = {
			m_VertexBuffer,
		};
		VkDeviceSize Offsets[]{ 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, VertexBuffers, Offsets);
		vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
		if (pDescriptorSet)
		{
			vkCmdBindDescriptorSets(commandBuffer,
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				pipelineLayout,
				0, 1,
				pDescriptorSet,
				0, NULL);
		}
		else
		{
			vkCmdBindDescriptorSets(commandBuffer,
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				pipelineLayout,
				0, 1,
				&m_DescriptorSet,
				0, NULL);
		}


		vkCmdDrawIndexed(commandBuffer, static_cast<UINT>(m_vecIndices.size()), 1, 0, 0, 0);
	}

	GLTFModel::GLTFModel(VulkanRenderer* pRenderer, const std::filesystem::path& filepath)
		: Model(pRenderer, filepath)
	{
		tinygltf::Model gltfModel;
		tinygltf::TinyGLTF loader;
		std::string strError;
		std::string strWarn;

		bool res = loader.LoadASCIIFromFile(&gltfModel, &strError, &strWarn, m_Filepath.string());
		ASSERT(res, std::format("Load glTF model {} failed", m_Filepath.string().c_str()));

		LoadImages(gltfModel);
		LoadSamplers(gltfModel);
		LoadTextures(gltfModel);
		LoadMaterials(gltfModel);
		LoadNodes(gltfModel);
		LoadMeshes(gltfModel);

		ASSERT((gltfModel.defaultScene != -1) && (gltfModel.scenes.size() > 0));
		tinygltf::Scene& gltfDefaultScene = gltfModel.scenes[gltfModel.defaultScene];
		for (UINT i = 0; i < gltfDefaultScene.nodes.size(); ++i)
		{
			int nNodeIdx = gltfDefaultScene.nodes[i];
			if (nNodeIdx == -1 || nNodeIdx >= gltfModel.nodes.size())
				continue;
			LoadNodeRelation(nullptr, nNodeIdx);
		}
	}

	GLTFModel::~GLTFModel()
	{
		//primitive中的descriptorSet由m_GLTFDescriptorPool负责释放

		for (auto& image : m_vecImages)
		{
			vkDestroyImage(m_pRenderer->m_LogicalDevice, image.m_Image, nullptr);
			vkDestroyImageView(m_pRenderer->m_LogicalDevice, image.m_ImageView, nullptr);
			vkFreeMemory(m_pRenderer->m_LogicalDevice, image.m_Memory, nullptr);
		}

		for (auto& sampler : m_vecSamplers)
		{
			vkDestroySampler(m_pRenderer->m_LogicalDevice, sampler.m_Sampler, nullptr);
		}

		vkDestroyBuffer(m_pRenderer->m_LogicalDevice, m_VertexBuffer, nullptr);
		vkFreeMemory(m_pRenderer->m_LogicalDevice, m_VertexBufferMemory, nullptr);

		vkDestroyBuffer(m_pRenderer->m_LogicalDevice, m_IndexBuffer, nullptr);
		vkFreeMemory(m_pRenderer->m_LogicalDevice, m_IndexBufferMemory, nullptr);
	}

	void GLTFModel::Draw(VkCommandBuffer& commandBuffer, VkPipeline& pipeline, VkPipelineLayout& pipelineLayout, VkDescriptorSet* pDescriptorSet)
	{
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		VkBuffer vertexBuffers[] = {
			m_VertexBuffer,
		};
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer, 0, VK_INDEX_TYPE_UINT32);

		for (UINT i = 0; i < m_DefaultScene.m_vecHeadNodes.size(); ++i)
		{
			DrawNode(m_DefaultScene.m_vecHeadNodes[i], commandBuffer, pipeline, pipelineLayout);
		}
	}

	void GLTFModel::DrawNode(int nNodeIdx, VkCommandBuffer& commandBuffer, VkPipeline& pipeline, VkPipelineLayout& pipelineLayout)
	{
		auto& node = m_vecNodes[nNodeIdx];
		if (node.m_nMeshIdx != -1)
		{
			auto& mesh = m_vecMeshes[node.m_nMeshIdx];
			for (UINT i = 0; i < mesh.vecPrimitives.size(); ++i)
			{
				auto& primitive = mesh.vecPrimitives[i];

				glm::mat4 nodeMatrix = node.modelMatrix;
				int nParentNodeIdx = node.m_ParentIdx;
				while (nParentNodeIdx != -1)
				{
					auto& parentNode = m_vecNodes[nParentNodeIdx];
					nodeMatrix = parentNode.modelMatrix * nodeMatrix;
					nParentNodeIdx = parentNode.m_ParentIdx;
				}

				glm::mat4 model, view, proj;
				model = nodeMatrix;
				view = m_pRenderer->m_Camera.GetViewMatrix();
				proj = m_pRenderer->m_Camera.GetProjMatrix();
				std::array<glm::mat4, 3> MVPPushConstants = { model, view, proj };
				vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
					0, sizeof(MVPPushConstants), MVPPushConstants.data());

				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 
					0, 1, &primitive.m_DescriptorSet, 0, nullptr);

				vkCmdDrawIndexed(commandBuffer, primitive.m_uiIndexCount, 1, primitive.m_uiFirstIndex, 0, 0);
			}
		}

		for (int nChildNodeIdx : node.m_vecChildren)
		{
			DrawNode(nChildNodeIdx, commandBuffer, pipeline, pipelineLayout);
		}
	}

	void GLTFModel::LoadImages(const tinygltf::Model& gltfModel)
	{
		m_vecImages.resize(gltfModel.images.size());
		for (size_t i = 0; i < gltfModel.images.size(); ++i)
		{
			const tinygltf::Image& gltfImage = gltfModel.images[i];
			const UCHAR* imageBuffer = nullptr;
			UCHAR* allocImageBuffer = nullptr;
			size_t imageBufferSize = 0;
			bool bDeleteBuffer = false;
			if (gltfImage.component == 3) //RGB 需要转为RGBA
			{
				imageBufferSize = gltfImage.width * gltfImage.height * 4;
				allocImageBuffer = new UCHAR[imageBufferSize];
				UCHAR* rgbaPtr = allocImageBuffer;
				const UCHAR* rgbPtr = &gltfImage.image[0];
				for (size_t p = 0; p < gltfImage.width * gltfImage.height; ++p)
				{
					rgbaPtr[0] = rgbPtr[0]; // R
					rgbaPtr[1] = rgbPtr[1]; // G
					rgbaPtr[2] = rgbPtr[2]; // B
					rgbaPtr[3] = 255;       // A

					rgbaPtr += 4;
					rgbPtr += 3;
				}
				imageBuffer = allocImageBuffer;
				bDeleteBuffer = true;
			}
			else if (gltfImage.component == 4) //RGBA
			{
				imageBuffer = &gltfImage.image[0];
				imageBufferSize = gltfImage.image.size();
				bDeleteBuffer = false;
			}
			else
			{
				ASSERT(false, "Unsupport gltf image type");
			}

			auto& image = m_vecImages[i];
			image.m_uiWidth = static_cast<UINT>(gltfImage.width);
			image.m_uiHeight = static_cast<UINT>(gltfImage.height);
			image.m_strName = gltfImage.uri;

			m_pRenderer->CreateImageAndBindMemory(image.m_uiWidth, image.m_uiHeight,
				1, 1, 1,
				VK_SAMPLE_COUNT_1_BIT,
				VK_FORMAT_R8G8B8A8_SRGB,
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				image.m_Image, image.m_Memory);

			//copy之前，将layout从初始的undefined转为transfer dst
			m_pRenderer->ChangeImageLayout(image.m_Image,
				VK_FORMAT_R8G8B8A8_SRGB,				//image format
				1,										//mipmap levels
				1,										//layers
				1,										//faces
				VK_IMAGE_LAYOUT_UNDEFINED,				//src layout
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);	//dst layout

			m_pRenderer->TransferImageDataByStageBuffer(imageBuffer, imageBufferSize, image.m_Image, image.m_uiWidth, image.m_uiHeight);
			
			if (bDeleteBuffer)
				delete[] imageBuffer;

			m_pRenderer->ChangeImageLayout(image.m_Image,
				VK_FORMAT_R8G8B8A8_SRGB,
				1,
				1,
				1,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

			image.m_ImageView = m_pRenderer->CreateImageView(image.m_Image,
				VK_FORMAT_R8G8B8A8_SRGB,	//格式为sRGB
				VK_IMAGE_ASPECT_COLOR_BIT,	//aspectFlags为COLOR_BIT
				1,
				1,
				1);
		}
	}

	void GLTFModel::LoadSamplers(const tinygltf::Model& gltfModel)
	{
		if (gltfModel.samplers.size() == 0)
			m_vecSamplers.resize(1);
		else
			m_vecSamplers.resize(gltfModel.samplers.size());

		for (UINT i = 0; i < m_vecSamplers.size(); ++i)
		{
			Sampler& sampler = m_vecSamplers[i];

			VkFilter minFilter = VK_FILTER_LINEAR;
			VkFilter magFilter = VK_FILTER_LINEAR;
			VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			VkSamplerAddressMode addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			VkSamplerAddressMode addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;

			if (gltfModel.samplers.size() > 0)
			{
				const tinygltf::Sampler& gltfSampler = gltfModel.samplers[i];
				std::tie(minFilter, magFilter, mipmapMode) = DZW_VulkanUtils::TinyGltfFilterToVulkan(gltfSampler.minFilter, gltfSampler.magFilter);
				addressModeU = DZW_VulkanUtils::TinyGltfWrapModeToVulkan(gltfSampler.wrapS);
				addressModeV = DZW_VulkanUtils::TinyGltfWrapModeToVulkan(gltfSampler.wrapT);
			}

			VkSamplerCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			createInfo.minFilter = minFilter;
			createInfo.magFilter = magFilter;

			createInfo.addressModeU = addressModeU;
			createInfo.addressModeV = addressModeV;
			createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

			createInfo.anisotropyEnable = VK_FALSE;
			if (createInfo.anisotropyEnable == VK_TRUE)
			{
				auto& physicalDeviceProperties = m_pRenderer->m_mapPhysicalDeviceInfo.at(m_pRenderer->m_PhysicalDevice).properties;
				createInfo.maxAnisotropy = physicalDeviceProperties.limits.maxSamplerAnisotropy;
			}

			createInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
			createInfo.unnormalizedCoordinates = VK_FALSE;

			createInfo.compareEnable = VK_FALSE;
			createInfo.compareOp = VK_COMPARE_OP_ALWAYS;

			createInfo.mipmapMode = mipmapMode;
			createInfo.mipLodBias = 0.f;
			createInfo.minLod = 0.f;
			createInfo.maxLod = 0.f;

			VULKAN_ASSERT(vkCreateSampler(m_pRenderer->m_LogicalDevice, &createInfo, nullptr, &sampler.m_Sampler), "Create sampler failed");
		}
	}

	void GLTFModel::LoadTextures(const tinygltf::Model& gltfModel)
	{
		m_vecTextures.resize(gltfModel.textures.size());
		for (size_t i = 0; i < gltfModel.textures.size(); ++i)
		{
			const tinygltf::Texture& gltfTexture = gltfModel.textures[i];

			Texture& texture = m_vecTextures[i];
			texture.m_nImageIdx = gltfTexture.source;
			texture.m_nSamplerIdx = gltfTexture.sampler;
		}
	}

	void GLTFModel::LoadMaterials(const tinygltf::Model& gltfModel)
	{
		m_vecMaterials.resize(gltfModel.materials.size());
		for (size_t i = 0; i < gltfModel.materials.size(); ++i)
		{
			const tinygltf::Material& gltfMaterial = gltfModel.materials[i];

			Material& material = m_vecMaterials[i];
			material.m_strName = gltfMaterial.name;
			for (UINT k = 0; k < 4; ++k)
				material.m_BaseColorFactor[k] = gltfMaterial.pbrMetallicRoughness.baseColorFactor[k];
			material.m_nBaseColotTextureIdx = gltfMaterial.pbrMetallicRoughness.baseColorTexture.index;

			material.m_fMetallicFactor = gltfMaterial.pbrMetallicRoughness.metallicFactor;
			material.m_fRoughnessFactor = gltfMaterial.pbrMetallicRoughness.roughnessFactor;
			material.m_nMetallicRoughnessTextureIdx = gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index;

			material.m_fNormalScale = gltfMaterial.normalTexture.scale;
			material.m_nNormalTextureIdx = gltfMaterial.normalTexture.index;

			material.m_fOcclusionStrength = gltfMaterial.occlusionTexture.strength;
			material.m_nOcclusionTextureIdx = gltfMaterial.occlusionTexture.index;

			for (UINT k = 0; k < 3; ++k)
				material.m_EmmisiveFactor[k] = gltfMaterial.emissiveFactor[k];
			material.m_nEmmisiveTextureIdx = gltfMaterial.emissiveTexture.index;
		}

	}

	void GLTFModel::LoadMeshes(const tinygltf::Model& gltfModel)
	{
		m_vecMeshes.resize(gltfModel.meshes.size());
		for (size_t k = 0; k < gltfModel.meshes.size(); ++k)
		{
			const tinygltf::Mesh& gltfMesh = gltfModel.meshes[k];
			Mesh& mesh = m_vecMeshes[k];
			mesh.strName = gltfMesh.name;
			
			for (size_t i = 0; i < gltfMesh.primitives.size(); i++)
			{
				const tinygltf::Primitive& glTFPrimitive = gltfMesh.primitives[i];
				uint32_t firstIndex = static_cast<uint32_t>(m_vecIndices.size());
				uint32_t vertexStart = static_cast<uint32_t>(m_vecVertices.size());
				uint32_t indexCount = 0;
				// Vertices
				{
					const float* positionBuffer = nullptr;
					const float* normalsBuffer = nullptr;
					const float* texCoordsBuffer = nullptr;
					size_t vertexCount = 0;

					// Get buffer data for vertex positions
					if (glTFPrimitive.attributes.find("POSITION") != glTFPrimitive.attributes.end()) {
						const tinygltf::Accessor& accessor = gltfModel.accessors[glTFPrimitive.attributes.find("POSITION")->second];
						const tinygltf::BufferView& view = gltfModel.bufferViews[accessor.bufferView];
						positionBuffer = reinterpret_cast<const float*>(&(gltfModel.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
						vertexCount = accessor.count;
					}
					// Get buffer data for vertex normals
					if (glTFPrimitive.attributes.find("NORMAL") != glTFPrimitive.attributes.end()) {
						const tinygltf::Accessor& accessor = gltfModel.accessors[glTFPrimitive.attributes.find("NORMAL")->second];
						const tinygltf::BufferView& view = gltfModel.bufferViews[accessor.bufferView];
						normalsBuffer = reinterpret_cast<const float*>(&(gltfModel.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
					}
					// Get buffer data for vertex texture coordinates
					// glTF supports multiple sets, we only load the first one
					if (glTFPrimitive.attributes.find("TEXCOORD_0") != glTFPrimitive.attributes.end()) {
						const tinygltf::Accessor& accessor = gltfModel.accessors[glTFPrimitive.attributes.find("TEXCOORD_0")->second];
						const tinygltf::BufferView& view = gltfModel.bufferViews[accessor.bufferView];
						texCoordsBuffer = reinterpret_cast<const float*>(&(gltfModel.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
					}

					// Append data to model's vertex buffer
					for (size_t v = 0; v < vertexCount; v++) 
					{
						Vertex3D vert{};
						vert.pos = glm::vec4(glm::make_vec3(&positionBuffer[v * 3]), 1.0f);
						vert.normal = glm::normalize(glm::vec3(normalsBuffer ? glm::make_vec3(&normalsBuffer[v * 3]) : glm::vec3(0.0f)));
						vert.texCoord = texCoordsBuffer ? glm::make_vec2(&texCoordsBuffer[v * 2]) : glm::vec3(0.0f);
						vert.color = glm::vec3(1.0f);

						if (true) //vulkan flipY
						{
							vert.pos.y *= -1.f;
							vert.normal.y *= -1.f;
						}

						m_vecVertices.push_back(vert);
					}
				}
				// Indices
				{
					const tinygltf::Accessor& accessor = gltfModel.accessors[glTFPrimitive.indices];
					const tinygltf::BufferView& bufferView = gltfModel.bufferViews[accessor.bufferView];
					const tinygltf::Buffer& buffer = gltfModel.buffers[bufferView.buffer];

					indexCount += static_cast<uint32_t>(accessor.count);

					// glTF supports different component types of indices
					switch (accessor.componentType) {
					case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
						const uint32_t* buf = reinterpret_cast<const uint32_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
						for (size_t index = 0; index < accessor.count; index++) {
							m_vecIndices.push_back(buf[index] + vertexStart);
						}
						break;
					}
					case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
						const uint16_t* buf = reinterpret_cast<const uint16_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
						for (size_t index = 0; index < accessor.count; index++) {
							m_vecIndices.push_back(buf[index] + vertexStart);
						}
						break;
					}
					case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
						const uint8_t* buf = reinterpret_cast<const uint8_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
						for (size_t index = 0; index < accessor.count; index++) {
							m_vecIndices.push_back(buf[index] + vertexStart);
						}
						break;
					}
					default:
						ASSERT(false, "Unsupport gltf model index type");
						return;
					}
				}
				Primitive primitive{};
				primitive.m_uiFirstIndex = firstIndex;
				primitive.m_uiIndexCount = indexCount;
				primitive.m_nMaterialIdx = glTFPrimitive.material;

				if (primitive.m_nMaterialIdx != -1)
				{
					auto& material = m_vecMaterials[primitive.m_nMaterialIdx];
					if (material.m_nBaseColotTextureIdx != -1 
						&& material.m_nNormalTextureIdx != -1 
						&& material.m_nMetallicRoughnessTextureIdx != -1)
					{
						auto& baseColorTexture = m_vecTextures[material.m_nBaseColotTextureIdx];
						auto& normalTexture = m_vecTextures[material.m_nNormalTextureIdx];
						auto& occlusionMetallicRoughnessTexture = m_vecTextures[material.m_nMetallicRoughnessTextureIdx]; //默认occlusionTexture与metallicRoughnessTexture一致

						auto& baseColorImage = m_vecImages[baseColorTexture.m_nImageIdx];
						auto& normalImage = m_vecImages[normalTexture.m_nImageIdx];
						auto& occlusionMetallicRoughnessImage = m_vecImages[occlusionMetallicRoughnessTexture.m_nImageIdx];

						auto& baseColorSampler = m_vecSamplers[0];
						if (baseColorTexture.m_nSamplerIdx != -1)
							baseColorSampler = m_vecSamplers[baseColorTexture.m_nSamplerIdx];
						auto& normalSampler = m_vecSamplers[0];
						if (normalTexture.m_nSamplerIdx != -1)
							normalSampler = m_vecSamplers[normalTexture.m_nSamplerIdx];
						auto& occlusionMetallicRoughnessSampler = m_vecSamplers[0];
						if (occlusionMetallicRoughnessTexture.m_nSamplerIdx != -1)
							occlusionMetallicRoughnessSampler = m_vecSamplers[occlusionMetallicRoughnessTexture.m_nSamplerIdx];

						VkDescriptorSetAllocateInfo allocInfo{};
						allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
						allocInfo.descriptorSetCount = 1;
						allocInfo.descriptorPool = m_pRenderer->m_GLTFDescriptorPool;
						allocInfo.pSetLayouts = &m_pRenderer->m_GLTFDescriptorSetLayout;

						VULKAN_ASSERT(vkAllocateDescriptorSets(m_pRenderer->m_LogicalDevice, &allocInfo, &(primitive.m_DescriptorSet)), "Allocate gltf desctiprot set failed");

						//baseColor sampler
						VkDescriptorImageInfo baseColorImageInfo{};
						baseColorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
						baseColorImageInfo.imageView = baseColorImage.m_ImageView;
						baseColorImageInfo.sampler = baseColorSampler.m_Sampler;
						VkWriteDescriptorSet baseColorSamplerWrite{};
						baseColorSamplerWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
						baseColorSamplerWrite.dstSet = primitive.m_DescriptorSet;
						baseColorSamplerWrite.dstBinding = 0;
						baseColorSamplerWrite.dstArrayElement = 0;
						baseColorSamplerWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
						baseColorSamplerWrite.descriptorCount = 1;
						baseColorSamplerWrite.pImageInfo = &baseColorImageInfo;

						//normal sampler
						VkDescriptorImageInfo normalImageInfo{};
						normalImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
						normalImageInfo.imageView = normalImage.m_ImageView;
						normalImageInfo.sampler = normalSampler.m_Sampler;
						VkWriteDescriptorSet normalSamplerWrite{};
						normalSamplerWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
						normalSamplerWrite.dstSet = primitive.m_DescriptorSet;
						normalSamplerWrite.dstBinding = 1;
						normalSamplerWrite.dstArrayElement = 0;
						normalSamplerWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
						normalSamplerWrite.descriptorCount = 1;
						normalSamplerWrite.pImageInfo = &normalImageInfo;

						//ouulusion+metallic+roughness sampler
						VkDescriptorImageInfo omrImageInfo{};
						omrImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
						omrImageInfo.imageView = occlusionMetallicRoughnessImage.m_ImageView;
						omrImageInfo.sampler = occlusionMetallicRoughnessSampler.m_Sampler;
						VkWriteDescriptorSet omrSamplerWrite{};
						omrSamplerWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
						omrSamplerWrite.dstSet = primitive.m_DescriptorSet;
						omrSamplerWrite.dstBinding = 2;
						omrSamplerWrite.dstArrayElement = 0;
						omrSamplerWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
						omrSamplerWrite.descriptorCount = 1;
						omrSamplerWrite.pImageInfo = &omrImageInfo;

						std::vector<VkWriteDescriptorSet> vecDescriptorWrite = {
							baseColorSamplerWrite,
							normalSamplerWrite,
							omrSamplerWrite,
						};

						vkUpdateDescriptorSets(m_pRenderer->m_LogicalDevice, static_cast<UINT>(vecDescriptorWrite.size()), vecDescriptorWrite.data(), 0, nullptr);
					}
				}
				
				mesh.vecPrimitives.push_back(primitive);
			}
		}

		ASSERT(m_vecVertices.size() > 0, "Vertex data empty");
		VkDeviceSize verticesSize = sizeof(m_vecVertices[0]) * m_vecVertices.size();
		m_pRenderer->CreateBufferAndBindMemory(verticesSize,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			m_VertexBuffer, m_VertexBufferMemory);
		m_pRenderer->TransferBufferDataByStageBuffer(m_vecVertices.data(), verticesSize, m_VertexBuffer);

		ASSERT(m_vecIndices.size() > 0, "Index data empty");
		VkDeviceSize indicesSize = sizeof(m_vecIndices[0]) * m_vecIndices.size();
		m_pRenderer->CreateBufferAndBindMemory(indicesSize,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			m_IndexBuffer, m_IndexBufferMemory);
		m_pRenderer->TransferBufferDataByStageBuffer(m_vecIndices.data(), indicesSize, m_IndexBuffer);
	}

	void GLTFModel::LoadNodes(const tinygltf::Model& gltfModel)
	{
		m_vecNodes.resize(gltfModel.nodes.size());
		for (size_t i = 0; i < gltfModel.nodes.size(); ++i)
		{
			const tinygltf::Node& gltfNode = gltfModel.nodes[i];
			Node& node = m_vecNodes[i];
			node.strName = gltfNode.name;
			node.m_nMeshIdx = gltfNode.mesh;
			node.m_nIdx = static_cast<int>(i);

			for (size_t j = 0; j < gltfNode.children.size(); ++j)
			{
				node.m_vecChildren.push_back(gltfNode.children[j]);
			}

			node.modelMatrix = glm::mat4(1.f);
			if (gltfNode.matrix.size() == 16)
			{
				node.modelMatrix = glm::make_mat4x4(gltfNode.matrix.data());
			}
			else
			{
				if (gltfNode.translation.size() == 3)
				{
					node.modelMatrix = glm::translate(node.modelMatrix, glm::vec3(glm::make_vec3(gltfNode.translation.data())));
				}
				if (gltfNode.rotation.size() == 4)
				{
					glm::quat q = glm::make_quat(gltfNode.rotation.data());
					node.modelMatrix *= glm::mat4(q);
				}
				if (gltfNode.scale.size() == 3)
				{
					node.modelMatrix = glm::scale(node.modelMatrix, glm::vec3(glm::make_vec3(gltfNode.scale.data())));
				}
			}

		}
	}

	void GLTFModel::LoadNodeRelation(Node* parentNode, int nNodeIdx)
	{
		auto& node = m_vecNodes[nNodeIdx];

		if (parentNode)
		{
			node.m_ParentIdx = parentNode->m_nIdx;
		}
		else
		{
			node.m_ParentIdx = -1;
			m_DefaultScene.m_vecHeadNodes.push_back(nNodeIdx);
		}

		for (int nIdx : node.m_vecChildren)
		{
			auto& childNode = m_vecNodes[nIdx];
			LoadNodeRelation(&node, childNode.m_nIdx);
		}
	}

}