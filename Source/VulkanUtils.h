#pragma once
#include "vulkan/vulkan.h"
#include <unordered_map>
#include <filesystem>

#include "tiny_gltf.h"

namespace DZW_VulkanUtils
{
	//std::unordered_map<VkMemoryPropertyFlags, std::string> g_mapMemoryPropertyFlagToName = {
	//	{VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,			"Device Local"},
	//	{VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,			"Host Visible"},
	//	{VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,			"Host Coherent"},
	//	{VK_MEMORY_PROPERTY_HOST_CACHED_BIT,			"Host Cached"},
	//	{VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT,		"Lazily Allocated"},
	//	{VK_MEMORY_PROPERTY_PROTECTED_BIT,				"Protected"},
	//	{VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD,	"Device Coherent AMD"},
	//	{VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD,	"Device Uncached AMD"},
	//	{VK_MEMORY_PROPERTY_RDMA_CAPABLE_BIT_NV,		"RDMA Capable Nvidia"},
	//};

	//std::string GetMemoryPropertyNameByFlags(VkMemoryPropertyFlags flags)
	//{
	//	std::string strName = "";
	//	for (auto pair : g_mapMemoryPropertyFlagToName)
	//	{
	//		if (flags & pair.first)
	//			strName = strName.empty() ? pair.second : strName + " | " + pair.second;
	//	}
	//	return strName;
	//}

	//std::unordered_map<VkMemoryHeapFlags, std::string> g_mapMemoryHeapFlagToName = {
	//	{VK_MEMORY_HEAP_DEVICE_LOCAL_BIT,			"Device Local"},
	//	{VK_MEMORY_HEAP_MULTI_INSTANCE_BIT,			"Multi Instance"},
	//	{VK_MEMORY_HEAP_MULTI_INSTANCE_BIT_KHR,		"Multi Instance KHR"},
	//};

	//std::string GetMemoryHeapNameByFlags(VkMemoryHeapFlags flags)
	//{
	//	std::string strName = "";
	//	for (auto pair : g_mapMemoryHeapFlagToName)
	//	{
	//		if (flags & pair.first)
	//			strName = strName.empty() ? pair.second : strName + " | " + pair.second;
	//	}
	//	return strName;
	//}

	//std::unordered_map<VkQueueFlagBits, std::string> g_mapQueueFlagToName = {
	//	{VK_QUEUE_GRAPHICS_BIT,				"Graphic"},
	//	{VK_QUEUE_COMPUTE_BIT,				"Compute"},
	//	{VK_QUEUE_TRANSFER_BIT,				"Transfer"},
	//	//{VK_QUEUE_SAPARSE_BINDING_BIT,		"Saparse Binding"},
	//	{VK_QUEUE_PROTECTED_BIT,			"Protected"},
	//	//{VK_QUEUE_VIDEO_DECODE_BIT_KHR,		"Video Decode KHR"},
	//	//{VK_QUEUE_OPTICAL_FLOW_BIT_NV,		"Optical Flow NV"},
	//};

	//std::string GetQueueFamilyNameByFlags(VkQueueFlags flags)
	//{
	//	std::string strName = "";
	//	for (auto pair : g_mapQueueFlagToName)
	//	{
	//		if (flags & pair.first)
	//			strName = strName.empty() ? pair.second : strName + " | " + pair.second;
	//	}
	//	return strName;
	//}

	std::string GetPhysicalDeviceTypeName(VkPhysicalDeviceType eType);

	std::vector<char> ReadShaderFile(const std::filesystem::path& filepath);
	VkShaderModule CreateShaderModule(VkDevice device, const std::vector<char>& vecBytecode);

	std::tuple<VkFilter, VkFilter, VkSamplerMipmapMode> TinyGltfFilterToVulkan(int tinygltfMinFilter, int tinygltfMagFilter);
	VkSamplerAddressMode TinyGltfWrapModeToVulkan(int tinygltfWrapMode);
}