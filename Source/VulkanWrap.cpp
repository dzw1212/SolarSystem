#include "VulkanWrap.h"
#include "VulkanRenderer.h"

#include "tiny_obj_loader.h"
#include "Log.h"
#include "VulkanUtils.h"

namespace DZW_VulkanWrap
{
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
			Log::Error("Unsupport model type");
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

					vert.normal = {
						attr.normals[3 * static_cast<UINT64>(idx.normal_index) + 0],
						attr.normals[3 * static_cast<UINT64>(idx.normal_index) + 1],
						attr.normals[3 * static_cast<UINT64>(idx.normal_index) + 2],
					};

					vert.color = { 1.f, 1.f, 1.f };

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

	void OBJModel::Draw(VkCommandBuffer& commandBuffer, VkPipeline& pipeline, VkPipelineLayout& pipelineLayout, VkDescriptorSet& descriptorSet)
	{
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		VkBuffer VertexBuffers[] = {
			m_VertexBuffer,
		};
		VkDeviceSize Offsets[]{ 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, VertexBuffers, Offsets);
		vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdBindDescriptorSets(commandBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipelineLayout,
			0, 1,
			&descriptorSet,
			0, NULL);

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
		LoadTextures(gltfModel);
		LoadMaterials(gltfModel);
	}

	GLTFModel::~GLTFModel()
	{

	}

	void GLTFModel::Draw(VkCommandBuffer& commandBuffer, VkPipeline& pipeline, VkPipelineLayout& pipelineLayout, VkDescriptorSet& descriptorSet)
	{

	}

	void GLTFModel::LoadImages(tinygltf::Model& gltfModel)
	{
		m_vecImages.resize(gltfModel.images.size());
		for (size_t i = 0; i < gltfModel.images.size(); ++i)
		{
			tinygltf::Image& gltfImage = gltfModel.images[i];
			UCHAR* imageBuffer = nullptr;
			size_t imageBufferSize = 0;
			bool bDeleteBuffer = false;
			if (gltfImage.component == 3) //RGB 需要转为RGBA
			{
				imageBufferSize = gltfImage.width * gltfImage.height * 4;
				imageBuffer = new UCHAR[imageBufferSize];
				UCHAR* rgbaPtr = imageBuffer;
				UCHAR* rgbPtr = &gltfImage.image[0];
				for (size_t p = 0; p < gltfImage.width * gltfImage.height; ++p)
				{
					rgbaPtr[0] = rgbPtr[0]; // R
					rgbaPtr[1] = rgbPtr[1]; // G
					rgbaPtr[2] = rgbPtr[2]; // B
					rgbaPtr[3] = 255;       // A

					rgbaPtr += 4;
					rgbPtr += 3;
				}
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
		}
	}

	void GLTFModel::LoadSamplers(tinygltf::Model& gltfModel)
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
				tinygltf::Sampler& gltfSampler = gltfModel.samplers[i];
				std::tie(minFilter, magFilter, mipmapMode) = DZW_VulkanUtils::TinyGltfFilterToVulkan(gltfSampler.minFilter, gltfSampler.magFilter);
				addressModeU = DZW_VulkanUtils::TinyGltfWrapModeToVulkan(gltfSampler.wrapS);
				addressModeV = DZW_VulkanUtils::TinyGltfWrapModeToVulkan(gltfSampler.wrapT);
			}

			VkSamplerCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			//设置过采样与欠采样时的采样方法，可以是nearest，linear，cubic等
			createInfo.minFilter = minFilter;
			createInfo.magFilter = magFilter;

			//设置纹理采样超出边界时的寻址模式，可以是repeat，mirror，clamp to edge，clamp to border等
			createInfo.addressModeU = addressModeU;
			createInfo.addressModeV = addressModeV;
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
			createInfo.mipmapMode = mipmapMode;
			createInfo.mipLodBias = 0.f;
			createInfo.minLod = 0.f;
			createInfo.maxLod = 0.f;

			VULKAN_ASSERT(vkCreateSampler(m_pRenderer->m_LogicalDevice, &createInfo, nullptr, &sampler.m_Sampler), "Create sampler failed");
		}
	}

	void GLTFModel::LoadTextures(tinygltf::Model& gltfModel)
	{
		m_vecTextures.resize(gltfModel.textures.size());
		for (size_t i = 0; i < gltfModel.textures.size(); ++i)
		{
			tinygltf::Texture& gltfTexture = gltfModel.textures[i];

			Texture& texture = m_vecTextures[i];
			texture.m_nImageIdx = gltfTexture.source;
			texture.m_nSamplerIdx = gltfTexture.sampler;
		}
	}

	void GLTFModel::LoadMaterials(tinygltf::Model& gltfModel)
	{
		m_vecImages;
		m_vecTextures;
		gltfModel.materials;
	}


 //   void Model::Node::LoadMesh(Model& model, const tinygltf::Model& gltfModel)
 //   {
	//	const tinygltf::Node& gltfNode = gltfModel.nodes[m_nIndex];

	//	m_Mesh.nIndex = gltfNode.mesh;

	//	if (m_Mesh.nIndex == -1)
	//		return;

	//	const tinygltf::Mesh& gltfMesh = gltfModel.meshes[m_Mesh.nIndex];
	//	for (size_t k = 0; k < gltfMesh.primitives.size(); ++k)
	//	{
	//		uint32_t vertexStart = static_cast<uint32_t>(model.m_vecVertices.size());

	//		DZW_VulkanWrap::Model::Primitive primitive;
	//		primitive.uiFirstIndex = static_cast<uint32_t>(model.m_vecIndices.size());


	//		const tinygltf::Primitive& gltfPrimitive = gltfMesh.primitives[k];

	//		//获取vertex数据
	//		//pos
	//		const int positionAccessorIndex = gltfPrimitive.attributes.find("POSITION")->second;
	//		const tinygltf::Accessor& positionAccessor = gltfModel.accessors[positionAccessorIndex];
	//		const tinygltf::BufferView& positionBufferView = gltfModel.bufferViews[positionAccessor.bufferView];
	//		const tinygltf::Buffer& positionBuffer = gltfModel.buffers[positionBufferView.buffer];
	//		const float* positions = reinterpret_cast<const float*>(&(positionBuffer.data[positionBufferView.byteOffset + positionAccessor.byteOffset]));

	//		//normal
	//		const int normalAccessorIndex = gltfPrimitive.attributes.find("NORMAL")->second;
	//		const tinygltf::Accessor& normalAccessor = gltfModel.accessors[normalAccessorIndex];
	//		const tinygltf::BufferView& normalBufferView = gltfModel.bufferViews[normalAccessor.bufferView];
	//		const tinygltf::Buffer& normalBuffer = gltfModel.buffers[normalBufferView.buffer];
	//		const float* normals = reinterpret_cast<const float*>(&(normalBuffer.data[normalBufferView.byteOffset + normalAccessor.byteOffset]));

	//		//texcoord
	//		const int texcoordAccessorIndex = gltfPrimitive.attributes.find("TEXCOORD_0")->second;
	//		const tinygltf::Accessor& texcoordAccessor = gltfModel.accessors[texcoordAccessorIndex];
	//		const tinygltf::BufferView& texcoordBufferView = gltfModel.bufferViews[texcoordAccessor.bufferView];
	//		const tinygltf::Buffer& texcoordBuffer = gltfModel.buffers[texcoordBufferView.buffer];
	//		const float* texcoords = reinterpret_cast<const float*>(&(texcoordBuffer.data[texcoordBufferView.byteOffset + texcoordAccessor.byteOffset]));

	//		for (size_t v = 0; v < positionAccessor.count; v++)
	//		{
	//			Vertex3D vert{};
	//			vert.pos = glm::make_vec3(&positions[v * 3]);
	//			vert.normal = glm::normalize(glm::vec3(normals ? glm::make_vec3(&normals[v * 3]) : glm::vec3(0.0f)));
	//			vert.texCoord = texcoords ? glm::make_vec2(&texcoords[v * 2]) : glm::vec3(0.0f);
	//			vert.color = glm::vec3(1.0f);
	//			model.m_vecVertices.push_back(vert);
	//		}

	//		//获取Index数据
	//		if (gltfPrimitive.indices >= 0)
	//		{
	//			UINT uiIndexCount = 0;
	//			const int indicesAccessorIndex = gltfPrimitive.indices;
	//			const tinygltf::Accessor& indicesAccessor = gltfModel.accessors[indicesAccessorIndex];
	//			const tinygltf::BufferView& indicesBufferView = gltfModel.bufferViews[indicesAccessor.bufferView];
	//			const tinygltf::Buffer& indicesBuffer = gltfModel.buffers[indicesBufferView.buffer];

	//			primitive.uiIndexCount += indicesAccessor.count;

	//			switch (indicesAccessor.componentType) {
	//			case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
	//				uint32_t* buf = new uint32_t[indicesAccessor.count];
	//				memcpy(buf, &indicesBuffer.data[indicesAccessor.byteOffset + indicesBufferView.byteOffset], indicesAccessor.count * sizeof(uint32_t));
	//				for (size_t index = 0; index < indicesAccessor.count; index++) {
	//					model.m_vecIndices.push_back(buf[index] + vertexStart);
	//				}
	//				delete[] buf;
	//				break;
	//			}
	//			case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
	//				uint16_t* buf = new uint16_t[indicesAccessor.count];
	//				memcpy(buf, &indicesBuffer.data[indicesAccessor.byteOffset + indicesBufferView.byteOffset], indicesAccessor.count * sizeof(uint16_t));
	//				for (size_t index = 0; index < indicesAccessor.count; index++) {
	//					model.m_vecIndices.push_back(buf[index] + vertexStart);
	//				}
	//				delete[] buf;
	//				break;
	//			}
	//			case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
	//				uint8_t* buf = new uint8_t[indicesAccessor.count];
	//				memcpy(buf, &indicesBuffer.data[indicesAccessor.byteOffset + indicesBufferView.byteOffset], indicesAccessor.count * sizeof(uint8_t));
	//				for (size_t index = 0; index < indicesAccessor.count; index++) {
	//					model.m_vecIndices.push_back(buf[index] + vertexStart);
	//				}
	//				delete[] buf;
	//				break;
	//			}
	//			default:
	//				ASSERT(false, "Unsupport index component type");
	//				return;
	//			}
	//		}

	//		m_Mesh.vecPrimitives.emplace_back(primitive);
	//	}
 //   }
	//void Model::LoadNode(Node* parentNode, Node* targetNode, int nNodeIdx, const tinygltf::Model& gltfModel)
	//{
	//	if (!targetNode)
	//		return;

	//	targetNode->m_nIndex = nNodeIdx;
	//	targetNode->m_Parent = parentNode;

	//	//直接关联的网格
	//	targetNode->LoadMesh(*this, gltfModel);

	//	//子节点
	//	const tinygltf::Node& gltfNode = gltfModel.nodes[targetNode->m_nIndex];
	//	targetNode->m_vecChildren.resize(gltfNode.children.size());
	//	for (size_t k = 0; k < gltfNode.children.size(); ++k)
	//	{
	//		LoadNode(targetNode, &(targetNode->m_vecChildren[k]), gltfNode.children[k], gltfModel);
	//		if (parentNode)
	//			parentNode->m_vecChildren.push_back(*targetNode);
	//	}

	//	
	//}

}