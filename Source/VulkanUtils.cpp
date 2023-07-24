#include "VulkanUtils.h"
#include "Core.h"

#include <fstream>

namespace DZW_VulkanUtils
{
	std::unordered_map<VkPhysicalDeviceType, std::string> g_mapPhysicalDeviceTypeToName = {
		 {VK_PHYSICAL_DEVICE_TYPE_OTHER,			"Other"},
		 {VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU,	"Integrated GPU"},
		 {VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU,		"Discrete GPU"},
		 {VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU,		"Virtual GPU"},
		 {VK_PHYSICAL_DEVICE_TYPE_CPU,				"CPU"},
	};

	std::string GetPhysicalDeviceTypeName(VkPhysicalDeviceType eType)
	{
		std::string strName;
		auto iter = g_mapPhysicalDeviceTypeToName.find(eType);
		if (iter != g_mapPhysicalDeviceTypeToName.end())
			strName = iter->second;
		return strName;
	}

	std::vector<char> ReadShaderFile(const std::filesystem::path& filepath)
	{
		ASSERT(std::filesystem::exists(filepath), std::format("Shader file path {} not exist", filepath.string()));
		//光标置于文件末尾，方便统计长度
		std::ifstream file(filepath, std::ios::ate | std::ios::binary);
		ASSERT(file.is_open(), std::format("Open shader file {} failed", filepath.string()));

		//tellg获取当前文件读写位置
		size_t fileSize = static_cast<size_t>(file.tellg());
		std::vector<char> vecBuffer(fileSize);

		//指针回到文件开头
		file.seekg(0);
		file.read(vecBuffer.data(), fileSize);

		file.close();
		return vecBuffer;
	}
	VkShaderModule CreateShaderModule(VkDevice device, const std::vector<char>& vecBytecode)
	{
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = vecBytecode.size();
		createInfo.pCode = reinterpret_cast<const UINT*>(vecBytecode.data());

		VkShaderModule shaderModule;
		VULKAN_ASSERT(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule), "Create shader module failed");

		return shaderModule;
	}
}