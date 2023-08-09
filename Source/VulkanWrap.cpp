#include "VulkanWrap.h"

namespace DZW_VulkanWrap
{
    void Model::Node::LoadMesh(Model& model, const tinygltf::Model& gltfModel)
    {
		const tinygltf::Node& gltfNode = gltfModel.nodes[m_nIndex];

		m_Mesh.nIndex = gltfNode.mesh;

		ASSERT(m_Mesh.nIndex != -1);

		const tinygltf::Mesh& gltfMesh = gltfModel.meshes[m_Mesh.nIndex];
		for (size_t k = 0; k < gltfMesh.primitives.size(); ++k)
		{
			uint32_t vertexStart = static_cast<uint32_t>(model.m_vecVertices.size());

			DZW_VulkanWrap::Model::Primitive primitive;
			primitive.uiFirstIndex = static_cast<uint32_t>(model.m_vecIndices.size());


			const tinygltf::Primitive& gltfPrimitive = gltfMesh.primitives[k];

			//获取vertex数据
			//pos
			const int positionAccessorIndex = gltfPrimitive.attributes.find("POSITION")->second;
			const tinygltf::Accessor& positionAccessor = gltfModel.accessors[positionAccessorIndex];
			const tinygltf::BufferView& positionBufferView = gltfModel.bufferViews[positionAccessor.bufferView];
			const tinygltf::Buffer& positionBuffer = gltfModel.buffers[positionBufferView.buffer];
			const float* positions = reinterpret_cast<const float*>(&(positionBuffer.data[positionBufferView.byteOffset + positionAccessor.byteOffset]));

			//normal
			const int normalAccessorIndex = gltfPrimitive.attributes.find("NORMAL")->second;
			const tinygltf::Accessor& normalAccessor = gltfModel.accessors[normalAccessorIndex];
			const tinygltf::BufferView& normalBufferView = gltfModel.bufferViews[normalAccessor.bufferView];
			const tinygltf::Buffer& normalBuffer = gltfModel.buffers[normalBufferView.buffer];
			const float* normals = reinterpret_cast<const float*>(&(normalBuffer.data[normalBufferView.byteOffset + normalAccessor.byteOffset]));

			//texcoord
			const int texcoordAccessorIndex = gltfPrimitive.attributes.find("TEXCOORD_0")->second;
			const tinygltf::Accessor& texcoordAccessor = gltfModel.accessors[texcoordAccessorIndex];
			const tinygltf::BufferView& texcoordBufferView = gltfModel.bufferViews[texcoordAccessor.bufferView];
			const tinygltf::Buffer& texcoordBuffer = gltfModel.buffers[texcoordBufferView.buffer];
			const float* texcoords = reinterpret_cast<const float*>(&(texcoordBuffer.data[texcoordBufferView.byteOffset + texcoordAccessor.byteOffset]));

			for (size_t v = 0; v < positionAccessor.count; v++)
			{
				Vertex3D vert{};
				vert.pos = glm::make_vec3(&positions[v * 3]);
				vert.normal = glm::normalize(glm::vec3(normals ? glm::make_vec3(&normals[v * 3]) : glm::vec3(0.0f)));
				vert.texCoord = texcoords ? glm::make_vec2(&texcoords[v * 2]) : glm::vec3(0.0f);
				vert.color = glm::vec3(1.0f);
				model.m_vecVertices.push_back(vert);
			}

			//获取Index数据
			if (gltfPrimitive.indices >= 0)
			{
				UINT uiIndexCount = 0;
				const int indicesAccessorIndex = gltfPrimitive.indices;
				const tinygltf::Accessor& indicesAccessor = gltfModel.accessors[indicesAccessorIndex];
				const tinygltf::BufferView& indicesBufferView = gltfModel.bufferViews[indicesAccessor.bufferView];
				const tinygltf::Buffer& indicesBuffer = gltfModel.buffers[indicesBufferView.buffer];

				primitive.uiIndexCount += indicesAccessor.count;

				switch (indicesAccessor.componentType) {
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
					uint32_t* buf = new uint32_t[indicesAccessor.count];
					memcpy(buf, &indicesBuffer.data[indicesAccessor.byteOffset + indicesBufferView.byteOffset], indicesAccessor.count * sizeof(uint32_t));
					for (size_t index = 0; index < indicesAccessor.count; index++) {
						model.m_vecIndices.push_back(buf[index] + vertexStart);
					}
					delete[] buf;
					break;
				}
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
					uint16_t* buf = new uint16_t[indicesAccessor.count];
					memcpy(buf, &indicesBuffer.data[indicesAccessor.byteOffset + indicesBufferView.byteOffset], indicesAccessor.count * sizeof(uint16_t));
					for (size_t index = 0; index < indicesAccessor.count; index++) {
						model.m_vecIndices.push_back(buf[index] + vertexStart);
					}
					delete[] buf;
					break;
				}
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
					uint8_t* buf = new uint8_t[indicesAccessor.count];
					memcpy(buf, &indicesBuffer.data[indicesAccessor.byteOffset + indicesBufferView.byteOffset], indicesAccessor.count * sizeof(uint8_t));
					for (size_t index = 0; index < indicesAccessor.count; index++) {
						model.m_vecIndices.push_back(buf[index] + vertexStart);
					}
					delete[] buf;
					break;
				}
				default:
					ASSERT(false, "Unsupport index component type");
					return;
				}
			}

			m_Mesh.vecPrimitives.emplace_back(primitive);
		}
    }
	Model::Node& Model::LoadNode(Node* parentNode, int nNodeIdx, const tinygltf::Model& gltfModel)
	{
		DZW_VulkanWrap::Model::Node node;
		node.m_nIndex = nNodeIdx;
		node.m_Parent = parentNode;

		//直接关联的网格
		node.LoadMesh(*this, gltfModel);

		//子节点
		const tinygltf::Node& gltfNode = gltfModel.nodes[node.m_nIndex];
		for (size_t k = 0; k < gltfNode.children.size(); ++k)
		{
			LoadNode(&node, gltfNode.children[k], gltfModel);
		}

		if (parentNode)
			parentNode->m_vecChildren.emplace_back(&node);

		return node;
	}
}