#include "Core.h"
#include "VulkanRenderer.h"
#include "VulkanUtils.h"
#include "Log.h"

#include <chrono>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"
#include "glm/detail/type_vec3.hpp"
#include "glm/vec4.hpp"
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
// #define TINYGLTF_NOEXCEPTION // optional. disable exception handling.

#include "tiny_gltf.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#define INSTANCE_NUM 2

#include "imgui.h"
#include "UI/UI.h"
static UI g_UI;


VulkanRenderer::VulkanRenderer()
{
#ifdef NDEBUG
	m_bEnableValidationLayer = false;
#else
	m_bEnableValidationLayer = true;
#endif

	m_uiWindowWidth = 1920;
	m_uiWindowHeight = 1080;
	m_strWindowTitle = "Vulkan Renderer";

	m_bFrameBufferResized = false;

	m_mapShaderPath = {
		{ VK_SHADER_STAGE_VERTEX_BIT,	"./Assert/Shader/vert.spv" },
		{ VK_SHADER_STAGE_FRAGMENT_BIT,	"./Assert/Shader/frag.spv" },
	};

	//m_TexturePath = "./Assert/Texture/Earth/8081_earthmap4k.jpg";
	//m_TexturePath = "./Assert/Texture/Earth2/8k_earth_daymap.jpg";
	//m_TexturePath = "./Assert/Texture/Earth2/8k_earth_clouds.jpg";
	m_TexturePath = "./Assert/Texture/viking_room.png";

	m_ModelPath = "./Assert/Model/viking_room.obj";

	m_uiMipmapLevel = 1;

	m_bViewportAndScissorIsDynamic = false;

	m_uiCurFrameIdx = 0;

	m_uiFPS = 0;
	m_uiFrameCounter = 0;

	m_DynamicAlignment = 0;
}

VulkanRenderer::~VulkanRenderer()
{
	Clean();
}

void VulkanRenderer::Init()
{
	InitWindow();
	CreateInstance();
	CreateWindowSurface();
	PickBestPhysicalDevice();
	CreateLogicalDevice();

	CreateTransferCommandPool();

	CreateSwapChain();
	CreateRenderPass();

	CreateSwapChainImages();
	CreateSwapChainImageViews();
	CreateSwapChainFrameBuffers();


	CreateShader();
	CreateUniformBuffers();
	CreateTextureSampler();

	CreateTextureImageAndFillData();
	CreateTextureImageView();

	CreateDescriptorSetLayout();
	CreateDescriptorPool();
	CreateDescriptorSets();

	CreateVertexBuffer();
	CreateIndexBuffer();

	CreateCommandPool();
	CreateCommandBuffer();

	CreateSyncObjects();

	CreateGraphicPipeline();

	SetupCamera();

	g_UI.Init(this);
}

void VulkanRenderer::Loop()
{
	while (!glfwWindowShouldClose(m_pWindow))
	{
		glfwPollEvents();

		static std::chrono::time_point<std::chrono::high_resolution_clock> lastTimestamp = std::chrono::high_resolution_clock::now();

		Render();

		auto nowTimestamp = std::chrono::high_resolution_clock::now();

		float fpsTimer = (float)(std::chrono::duration<double, std::milli>(nowTimestamp - lastTimestamp).count());
		if (fpsTimer > 1000.0f)
		{
			m_uiFPS = static_cast<uint32_t>((float)m_uiFrameCounter * (1000.0f / fpsTimer));
			m_uiFrameCounter = 0;
			lastTimestamp = nowTimestamp;
		}
	}

	//等待GPU将当前的命令执行完成，资源未被占用时才能销毁
	vkDeviceWaitIdle(m_LogicalDevice);
}

void VulkanRenderer::Clean()
{
	g_UI.Clean();

	for (const auto& shaderModule : m_mapShaderModule)
	{
		vkDestroyShaderModule(m_LogicalDevice, shaderModule.second, nullptr);
	}

	vkDestroyImageView(m_LogicalDevice, m_DepthImageView, nullptr);
	vkDestroyImage(m_LogicalDevice, m_DepthImage, nullptr);
	vkFreeMemory(m_LogicalDevice, m_DepthImageMemory, nullptr);

	for (const auto& frameBuffer : m_vecSwapChainFrameBuffers)
	{
		vkDestroyFramebuffer(m_LogicalDevice, frameBuffer, nullptr);
	}

	for (const auto& imageView : m_vecSwapChainImageViews)
	{
		vkDestroyImageView(m_LogicalDevice, imageView, nullptr);
	}

	//for (const auto& image : m_vecSwapChainImages)
	//{
	//	vkDestroyImage(m_LogicalDevice, image, nullptr);
	//}

	//清理SwapChain
	vkDestroySwapchainKHR(m_LogicalDevice, m_SwapChain, nullptr);

	vkDestroySampler(m_LogicalDevice, m_TextureSampler, nullptr);
	vkDestroyImageView(m_LogicalDevice, m_TextureImageView, nullptr);
	vkDestroyImage(m_LogicalDevice, m_TextureImage, nullptr);
	vkFreeMemory(m_LogicalDevice, m_TextureImageMemory, nullptr);

	vkDestroyDescriptorPool(m_LogicalDevice, m_DescriptorPool, nullptr);

	vkDestroyDescriptorSetLayout(m_LogicalDevice, m_DescriptorSetLayout, nullptr);
	for (size_t i = 0; i < m_vecSwapChainImages.size(); ++i)
	{
		vkFreeMemory(m_LogicalDevice, m_vecDynamicUniformBufferMemories[i], nullptr);
		vkDestroyBuffer(m_LogicalDevice, m_vecDynamicUniformBuffers[i], nullptr);
	}


	vkFreeMemory(m_LogicalDevice, m_VertexBufferMemory, nullptr);
	vkDestroyBuffer(m_LogicalDevice, m_VertexBuffer, nullptr);

	if (m_Indices.size() > 0)
	{
		vkFreeMemory(m_LogicalDevice, m_IndexBufferMemory, nullptr);
		vkDestroyBuffer(m_LogicalDevice, m_IndexBuffer, nullptr);
	}

	for (int i = 0; i < m_vecSwapChainImages.size(); ++i)
	{
		vkDestroySemaphore(m_LogicalDevice, m_vecImageAvailableSemaphores[i], nullptr);
		vkDestroySemaphore(m_LogicalDevice, m_vecRenderFinishedSemaphores[i], nullptr);
		vkDestroyFence(m_LogicalDevice, m_vecInFlightFences[i], nullptr);
	}

	vkDestroyCommandPool(m_LogicalDevice, m_CommandPool, nullptr);
	vkDestroyCommandPool(m_LogicalDevice, m_TransferCommandPool, nullptr);

	if (m_bEnableValidationLayer)
	{
		DestoryDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);
	}

	vkDestroyPipeline(m_LogicalDevice, m_GraphicPipeline, nullptr);
	vkDestroyPipelineLayout(m_LogicalDevice, m_GraphicPipelineLayout, nullptr);
	vkDestroyRenderPass(m_LogicalDevice, m_RenderPass, nullptr);

	vkDestroySurfaceKHR(m_Instance, m_WindowSurface, nullptr);
	vkDestroyDevice(m_LogicalDevice, nullptr);

	vkDestroyInstance(m_Instance, nullptr);

	glfwDestroyWindow(m_pWindow);
	glfwTerminate();
}

void VulkanRenderer::LoadOBJ(const std::filesystem::path& modelPath)
{
	tinyobj::attrib_t attr;	//存储所有顶点、法线、UV坐标
	std::vector<tinyobj::shape_t> vecShapes;
	std::vector<tinyobj::material_t> vecMaterials;
	std::string strWarning;
	std::string strError;

	bool res = tinyobj::LoadObj(&attr, &vecShapes, &vecMaterials, &strWarning, &strError, modelPath.string().c_str());
	ASSERT(res, std::format("Load obj file {} failed", modelPath.string().c_str()));

	m_Vertices.clear();
	m_Indices.clear();

	std::unordered_map<Vertex3D, uint32_t> mapUniqueVertices;

	for (const auto& shape : vecShapes)
	{
		for (const auto& index : shape.mesh.indices)
		{
			Vertex3D vert{};
			vert.pos = {
				attr.vertices[3 * static_cast<uint64_t>(index.vertex_index) + 0],
				attr.vertices[3 * static_cast<uint64_t>(index.vertex_index) + 1],
				attr.vertices[3 * static_cast<uint64_t>(index.vertex_index) + 2],
			};
			vert.texCoord = {
				attr.texcoords[2 * static_cast<uint64_t>(index.texcoord_index) + 0],
				1.f - attr.texcoords[2 * static_cast<uint64_t>(index.texcoord_index) + 1],
			};
			vert.color = glm::vec3(1.f, 1.f, 1.f);

			m_Vertices.push_back(vert);
			m_Indices.push_back(static_cast<uint32_t>(m_Indices.size()));
		}
	}
}

void VulkanRenderer::LoadGLTF(const std::filesystem::path& modelPath)
{
	tinygltf::Model model;
	tinygltf::TinyGLTF loader;

	std::string err;
	std::string warn;
	bool ret = false;

	if (modelPath.extension().string() == ".glb")
		ret = loader.LoadBinaryFromFile(&model, &err, &warn, modelPath.string());
	else if (modelPath.extension().string() == ".gltf")
		ret = loader.LoadASCIIFromFile(&model, &err, &warn, modelPath.string());

	if (!warn.empty())
		Log::Warn(std::format("Load gltf warn: {}", warn.c_str()));

	if (!err.empty())
		Log::Error(std::format("Load gltf error: {}", err.c_str()));

	ASSERT(ret, "Load gltf failed");

	for (auto& mesh : model.meshes)
	{
		for (auto& primitive : mesh.primitives)
		{
			const auto& accessor = model.accessors[primitive.attributes["POSITION"]];
			const auto& bufferView = model.bufferViews[accessor.bufferView];
			const auto& buffer = model.buffers[bufferView.buffer];
			const float* positions = reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
			for (size_t i = 0; i < accessor.count; ++i)
			{
				Vertex3D point;
				point.pos.x = positions[i * 3 + 0];
				point.pos.y = positions[i * 3 + 1];
				point.pos.z = positions[i * 3 + 2];

				point.color = { 1.0, 0.0, 0.0 };

				m_Vertices.push_back(point);
			}
		}

		uint32_t indexStart = static_cast<uint32_t>(m_Indices.size());
		uint32_t vertexStart = static_cast<uint32_t>(m_Vertices.size());

		for (auto& primitive : mesh.primitives)
		{
			if (primitive.indices == -1)
				continue;
			const auto& accessor = model.accessors[primitive.indices];
			const auto& bufferView = model.bufferViews[accessor.bufferView];
			const auto& buffer = model.buffers[bufferView.buffer];

			switch (accessor.componentType) {
			case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
				uint32_t* buf = new uint32_t[accessor.count];
				memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint32_t));
				for (size_t index = 0; index < accessor.count; index++) {
					m_Indices.push_back(buf[index] + vertexStart);
				}
				delete[] buf;
				break;
			}
			case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
				uint16_t* buf = new uint16_t[accessor.count];
				memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint16_t));
				for (size_t index = 0; index < accessor.count; index++) {
					m_Indices.push_back(buf[index] + vertexStart);
				}
				delete[] buf;
				break;
			}
			case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
				uint8_t* buf = new uint8_t[accessor.count];
				memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint8_t));
				for (size_t index = 0; index < accessor.count; index++) {
					m_Indices.push_back(buf[index] + vertexStart);
				}
				delete[] buf;
				break;
			}
			default:
				ASSERT(false, "Unsupport index component type");
				return;
			}
		}
	}
}

void VulkanRenderer::LoadModel(const std::filesystem::path& modelPath)
{
	const auto& extensionName = modelPath.extension().string();
	if (extensionName == ".obj")
		LoadOBJ(modelPath);
	else if (extensionName == ".gltf" || extensionName == ".glb")
		LoadGLTF(modelPath);
	else
		ASSERT(false, "Unsupport model type");
}

void VulkanRenderer::FrameBufferResizeCallBack(GLFWwindow* pWindow, int nWidth, int nHeight)
{
	auto vulkanRenderer = reinterpret_cast<VulkanRenderer*>(glfwGetWindowUserPointer(pWindow));
	if (vulkanRenderer)
		vulkanRenderer->m_bFrameBufferResized = true;
}

void VulkanRenderer::InitWindow()
{
	glfwInit();
	ASSERT(glfwVulkanSupported(), "GLFW version not support vulkan");

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	m_pWindow = glfwCreateWindow(m_uiWindowWidth, m_uiWindowHeight, m_strWindowTitle.c_str(), nullptr, nullptr);
	glfwMakeContextCurrent(m_pWindow);

	glfwSetWindowUserPointer(m_pWindow, this);
	glfwSetFramebufferSizeCallback(m_pWindow, FrameBufferResizeCallBack);
}

void VulkanRenderer::QueryGLFWExtensions()
{
	const char** ppGLFWEntensions = nullptr;
	UINT uiCount = 0;
	ppGLFWEntensions = glfwGetRequiredInstanceExtensions(&uiCount);

	m_vecChosedExtensions = std::vector<const char*>(ppGLFWEntensions, ppGLFWEntensions + uiCount);
}

void VulkanRenderer::QueryValidationLayerExtensions()
{
	m_vecChosedExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
}

void VulkanRenderer::QueryAllValidExtensions()
{
	UINT uiExtensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &uiExtensionCount, nullptr);

	ASSERT(uiExtensionCount > 0, "Find no valid extensions");

	m_vecValidExtensions.resize(uiExtensionCount);

	vkEnumerateInstanceExtensionProperties(nullptr, &uiExtensionCount, m_vecValidExtensions.data());
}

bool VulkanRenderer::IsExtensionValid(const std::string& strExtensionName)
{
	for (const auto& extensionProperty : m_vecValidExtensions)
	{
		if (extensionProperty.extensionName == strExtensionName)
			return true;
	}
	return false;
}

bool VulkanRenderer::CheckChosedExtensionValid()
{
	QueryGLFWExtensions();
	QueryValidationLayerExtensions();

	QueryAllValidExtensions();

	for (const auto& extensionName : m_vecChosedExtensions)
	{
		if (!IsExtensionValid(extensionName))
			return false;
	}
	return true;
}

void VulkanRenderer::QueryValidationLayers()
{
	m_vecChosedValidationLayers.push_back("VK_LAYER_KHRONOS_validation");
}

void VulkanRenderer::QueryAllValidValidationLayers()
{
	uint32_t uiLayerCount = 0;
	vkEnumerateInstanceLayerProperties(&uiLayerCount, nullptr);

	ASSERT(uiLayerCount > 0, "Find no valid validation layer");

	m_vecValidValidationLayers.resize(uiLayerCount);
	vkEnumerateInstanceLayerProperties(&uiLayerCount, m_vecValidValidationLayers.data());
}

bool VulkanRenderer::IsValidationLayerValid(const std::string& strValidationLayerName)
{
	for (const auto& layerProperty : m_vecValidValidationLayers)
	{
		if (layerProperty.layerName == strValidationLayerName)
			return true;
	}
	return false;
}

bool VulkanRenderer::CheckChosedValidationLayerValid()
{
	QueryValidationLayers();

	QueryAllValidValidationLayers();

	for (const auto& layerName : m_vecChosedValidationLayers)
	{
		if (!IsValidationLayerValid(layerName))
			return false;
	}
	return true;
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanRenderer::debugCallBack(VkDebugUtilsMessageSeverityFlagBitsEXT messageServerity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
	auto msg = std::format("[ValidationLayer] {}", pCallbackData->pMessage);
	switch (messageServerity)
	{
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
		Log::Trace(msg);
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
		Log::Info(msg);
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
		Log::Warn(msg);
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
		Log::Error(msg);
		break;
	default:
		ASSERT(false, "Unsupport validation layer call back message serverity");
		break;
	}
	return VK_FALSE;
}

VkResult VulkanRenderer::CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
{
	//属于Extension的函数，不能直接使用，需要先封装一层，使用vkGetInstanceProcAddr判断该函数是否可用
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func)
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	else
		return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void VulkanRenderer::DestoryDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
{
	//属于Extension的函数，不能直接使用，需要先封装一层，使用vkGetInstanceProcAddr判断该函数是否可用
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func)
		func(instance, debugMessenger, pAllocator);
}

void VulkanRenderer::FillDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& messengerCreateInfo)
{
	messengerCreateInfo = {};
	messengerCreateInfo.sType =
		VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	messengerCreateInfo.messageSeverity =
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT/* |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT*/;
	messengerCreateInfo.messageType =
		VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
	messengerCreateInfo.pfnUserCallback = debugCallBack;
}

void VulkanRenderer::SetupDebugMessenger(const VkDebugUtilsMessengerCreateInfoEXT& messengerCreateInfo)
{
	VULKAN_ASSERT(CreateDebugUtilsMessengerEXT(m_Instance, &messengerCreateInfo, nullptr, &m_DebugMessenger), "Setup debug messenger failed");
}

void VulkanRenderer::CreateInstance()
{
	CheckChosedExtensionValid();
	if (m_bEnableValidationLayer)
		CheckChosedValidationLayerValid();

	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = m_strWindowTitle.c_str();
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 3, 0);
	appInfo.pEngineName = nullptr;
	appInfo.engineVersion = VK_MAKE_VERSION(1, 3, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	createInfo.enabledExtensionCount = static_cast<UINT>(m_vecChosedExtensions.size());
	createInfo.ppEnabledExtensionNames = m_vecChosedExtensions.data();

	VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo{};

	if (m_bEnableValidationLayer)
	{
		createInfo.enabledLayerCount = static_cast<int>(m_vecChosedValidationLayers.size());
		createInfo.ppEnabledLayerNames = m_vecChosedValidationLayers.data();
		
		FillDebugMessengerCreateInfo(debugMessengerCreateInfo);
		createInfo.pNext = &debugMessengerCreateInfo;
	}
	else
	{
		createInfo.enabledLayerCount = 0;
		createInfo.ppEnabledLayerNames = nullptr;
		createInfo.pNext = nullptr;
	}

	VULKAN_ASSERT(vkCreateInstance(&createInfo, nullptr, &m_Instance), "Create instance failed");

	if (m_bEnableValidationLayer)
		SetupDebugMessenger(debugMessengerCreateInfo);
}

void VulkanRenderer::CreateWindowSurface()
{
	VULKAN_ASSERT(glfwCreateWindowSurface(m_Instance, m_pWindow, nullptr, &m_WindowSurface), "Create window surface failed");
}

void VulkanRenderer::QueryAllValidPhysicalDevice()
{
	UINT uiPhysicalDeviceCount = 0;
	vkEnumeratePhysicalDevices(m_Instance, &uiPhysicalDeviceCount, nullptr);
	
	ASSERT(uiPhysicalDeviceCount > 0, "Find no valid physical device");

	m_vecValidPhysicalDevices.resize(uiPhysicalDeviceCount);
	vkEnumeratePhysicalDevices(m_Instance, &uiPhysicalDeviceCount, m_vecValidPhysicalDevices.data());

	for (const auto& physicalDevice : m_vecValidPhysicalDevices)
	{
		PhysicalDeviceInfo info;
		
		vkGetPhysicalDeviceProperties(physicalDevice, &info.properties);
		vkGetPhysicalDeviceFeatures(physicalDevice, &info.features);

		UINT uiQueueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &uiQueueFamilyCount, nullptr);
		if (uiQueueFamilyCount > 0)
		{
			info.vecQueueFamilies.resize(uiQueueFamilyCount);
			vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &uiQueueFamilyCount, info.vecQueueFamilies.data());
		}

		info.strDeviceTypeName = VulkanUtils::GetPhysicalDeviceTypeName(info.properties.deviceType);

		int nIdx = 0;
		for (const auto& queueFamily : info.vecQueueFamilies)
		{
			if ((queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) && (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT))
			{
				info.graphicFamilyIdx = nIdx;
				break;
			}
			nIdx++;
		}

		nIdx = 0;
		VkBool32 bPresentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, nIdx, m_WindowSurface, &bPresentSupport);
		if (bPresentSupport)
			info.presentFamilyIdx = nIdx;

		//获取硬件支持的capability，包含Image数量上下限等信息
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, m_WindowSurface, &info.swapChainSupportInfo.capabilities);

		//获取硬件支持的Surface Format列表
		UINT uiFormatCount = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_WindowSurface, &uiFormatCount, nullptr);
		if (uiFormatCount > 0)
		{
			info.swapChainSupportInfo.vecSurfaceFormats.resize(uiFormatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_WindowSurface, &uiFormatCount, info.swapChainSupportInfo.vecSurfaceFormats.data());
		}

		//获取硬件支持的Present Mode列表
		UINT uiPresentModeCount = 0;
		vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_WindowSurface, &uiPresentModeCount, nullptr);
		if (uiPresentModeCount > 0)
		{
			info.swapChainSupportInfo.vecPresentModes.resize(uiPresentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_WindowSurface, &uiPresentModeCount, info.swapChainSupportInfo.vecPresentModes.data());
		}

		UINT uiExtensionCount = 0;
		vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &uiExtensionCount, nullptr);
		if (uiExtensionCount > 0)
		{
			info.vecAvaliableDeviceExtensions.resize(uiExtensionCount);
			vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &uiExtensionCount, info.vecAvaliableDeviceExtensions.data());
		}

		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &info.memoryProperties);

		info.nRateScore = RatePhysicalDevice(info);

		m_mapPhysicalDeviceInfo[physicalDevice] = info;
	}
}

int VulkanRenderer::RatePhysicalDevice(PhysicalDeviceInfo& deviceInfo)
{
	int nScore = 0;

	//检查是否是独立显卡
	if (deviceInfo.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		nScore += 1000;

	//检查支持的最大图像尺寸,越大越好
	nScore += deviceInfo.properties.limits.maxImageDimension2D;

	//检查是否支持几何着色器
	if (!deviceInfo.features.geometryShader)
		return 0;

	//检查是否支持Graphic Family, Present Family且Index一致
	if (!deviceInfo.IsGraphicAndPresentQueueFamilySame())
		return 0;

	//检查是否支持指定的Extension(Swap Chain等)
	bool bExtensionSupport = checkDeviceExtensionSupport(deviceInfo);
	if (!bExtensionSupport)
		return 0;

	////检查Swap Chain是否满足要求
	//bool bSwapChainAdequate = false;
	//if (bExtensionSupport) //首先确认支持SwapChain
	//{
	//	SwapChainSupportDetails details = querySwapChainSupport(device);
	//	bSwapChainAdequate = !details.vecSurfaceFormats.empty() && !details.vecPresentModes.empty();
	//}
	//if (!bSwapChainAdequate)
	//	return false;

	////检查是否支持各向异性过滤
	//if (!deviceFeatures.samplerAnisotropy)
	//	return false;

	return nScore;
}

void VulkanRenderer::PickBestPhysicalDevice()
{
	QueryAllValidPhysicalDevice();

	VkPhysicalDevice bestDevice;
	int nScore = 0;

	for (const auto& iter : m_mapPhysicalDeviceInfo)
	{
		if (iter.second.nRateScore > nScore)
		{
			nScore = iter.second.nRateScore;
			bestDevice = iter.first;
		}
	}

	m_PhysicalDevice = bestDevice;
}

bool VulkanRenderer::checkDeviceExtensionSupport(const PhysicalDeviceInfo& deviceInfo)
{
	std::set<std::string> setRequiredExtensions(m_vecDeviceExtensions.begin(), m_vecDeviceExtensions.end());

	for (const auto& extension : deviceInfo.vecAvaliableDeviceExtensions)
	{
		setRequiredExtensions.erase(extension.extensionName);
	}

	return setRequiredExtensions.empty();
}

void VulkanRenderer::CreateLogicalDevice()
{
	std::vector<VkDeviceQueueCreateInfo> vecQueueCreateInfo;

	auto iter = m_mapPhysicalDeviceInfo.find(m_PhysicalDevice);
	ASSERT((iter != m_mapPhysicalDeviceInfo.end()), "Cant find physical device info");

	const auto& physicalDeviceInfo = iter->second;
	float queuePriority = 1.f;

	VkDeviceQueueCreateInfo queueCreateInfo{};
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.queueFamilyIndex = physicalDeviceInfo.graphicFamilyIdx.value();
	queueCreateInfo.queueCount = 1;
	queueCreateInfo.pQueuePriorities = &queuePriority; //Queue的优先级，范围[0.f, 1.f]，控制CommandBuffer的执行顺序
	vecQueueCreateInfo.push_back(queueCreateInfo);

	VkPhysicalDeviceFeatures deviceFeatures{};
	//deviceFeatures.samplerAnisotropy = VK_TRUE; //启用各向异性过滤，用于纹理采样
	//deviceFeatures.sampleRateShading = VK_TRUE;	//启用Sample Rate Shaing，用于MSAA抗锯齿

	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.queueCreateInfoCount = static_cast<UINT>(vecQueueCreateInfo.size());
	createInfo.pQueueCreateInfos = vecQueueCreateInfo.data();
	createInfo.pEnabledFeatures = &deviceFeatures;
	createInfo.enabledExtensionCount = static_cast<UINT>(m_vecDeviceExtensions.size()); //注意！此处的extension与创建Instance时不同
	createInfo.ppEnabledExtensionNames = m_vecDeviceExtensions.data();
	if (m_bEnableValidationLayer)
	{
		createInfo.enabledLayerCount = static_cast<UINT>(m_vecChosedValidationLayers.size());
		createInfo.ppEnabledLayerNames = m_vecChosedValidationLayers.data();
	}
	else
	{
		createInfo.enabledLayerCount = 0;
		createInfo.ppEnabledLayerNames = nullptr;
	}

	VULKAN_ASSERT(vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_LogicalDevice), "Create logical device failed");

	vkGetDeviceQueue(m_LogicalDevice, physicalDeviceInfo.graphicFamilyIdx.value(), 0, &m_GraphicQueue);
	vkGetDeviceQueue(m_LogicalDevice, physicalDeviceInfo.presentFamilyIdx.value(), 0, &m_PresentQueue);
}

VkSurfaceFormatKHR VulkanRenderer::ChooseSwapChainSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& vecAvailableFormats)
{
	ASSERT(vecAvailableFormats.size() > 0, "No avaliable swap chain surface format");

	//找到支持sRGB格式的format，如果没有则返回第一个
	for (const auto& format : vecAvailableFormats)
	{
		if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return format;
		}
	}

	return vecAvailableFormats[0];
}

VkSurfaceFormatKHR VulkanRenderer::ChooseUISwapChainSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& vecAvailableFormats)
{
	ASSERT(vecAvailableFormats.size() > 0, "No avaliable UI swap chain surface format");

	//找到支持UNORM格式的format，如果没有则返回第一个
	for (const auto& format : vecAvailableFormats)
	{
		if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return format;
		}
	}

	return vecAvailableFormats[0];
}

VkPresentModeKHR VulkanRenderer::ChooseSwapChainPresentMode(const std::vector<VkPresentModeKHR>& vecAvailableModes)
{
	ASSERT(vecAvailableModes.size() > 0, "No avaliable swap chain present mode");

	//优先选择MAILBOX，其次FIFO
	for (const auto& mode : vecAvailableModes)
	{
		if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
			return mode;
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanRenderer::ChooseSwapChainSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
	if (capabilities.currentExtent.width != std::numeric_limits<UINT>::max())
		return capabilities.currentExtent;

	int nWidth = 0;
	int nHeight = 0;
	glfwGetFramebufferSize(m_pWindow, &nWidth, &nHeight);

	VkExtent2D actualExtent = { static_cast<UINT>(nWidth), static_cast<UINT>(nHeight) };

	actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
	actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

	return actualExtent;
}

UINT VulkanRenderer::ChooseSwapChainImageCount(const VkSurfaceCapabilitiesKHR& capabilities)
{
	UINT uiImageCount = capabilities.minImageCount + 1;
	if (capabilities.maxImageCount > 0)
	{
		uiImageCount = std::clamp(uiImageCount, capabilities.minImageCount, capabilities.maxImageCount);
	}
	return uiImageCount;
}

void VulkanRenderer::CreateSwapChain()
{
	const auto& physicalDeviceInfo = m_mapPhysicalDeviceInfo.at(m_PhysicalDevice);

	m_SwapChainSurfaceFormat = ChooseSwapChainSurfaceFormat(physicalDeviceInfo.swapChainSupportInfo.vecSurfaceFormats);
	m_SwapChainFormat = m_SwapChainSurfaceFormat.format;
	m_SwapChainPresentMode = ChooseSwapChainPresentMode(physicalDeviceInfo.swapChainSupportInfo.vecPresentModes);

	VkSurfaceCapabilitiesKHR capabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_PhysicalDevice, m_WindowSurface, &capabilities);
	m_SwapChainExtent2D = ChooseSwapChainSwapExtent(capabilities);

	m_uiSwapChainMinImageCount = ChooseSwapChainImageCount(physicalDeviceInfo.swapChainSupportInfo.capabilities);

	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = m_WindowSurface;
	createInfo.minImageCount = m_uiSwapChainMinImageCount;
	createInfo.imageFormat = m_SwapChainFormat;
	createInfo.imageColorSpace = m_SwapChainSurfaceFormat.colorSpace;
	createInfo.imageExtent = m_SwapChainExtent2D;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	createInfo.presentMode = m_SwapChainPresentMode;

	//挑选显卡时已确保GraphicFamily与PresentFamily一致
	createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	createInfo.queueFamilyIndexCount = 0;
	createInfo.pQueueFamilyIndices = nullptr;

	//设置如何进行Transform
	createInfo.preTransform = physicalDeviceInfo.swapChainSupportInfo.capabilities.currentTransform; //不做任何Transform

	//设置是否启用Alpha通道
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; //不启用Alpha通道

	//设置是否启用隐藏面剔除
	createInfo.clipped = VK_TRUE;

	//oldSwapChain用于window resize时
	createInfo.oldSwapchain = VK_NULL_HANDLE;

	VULKAN_ASSERT(vkCreateSwapchainKHR(m_LogicalDevice, &createInfo, nullptr, &m_SwapChain), "Create swap chain failed");
}

VkImageView VulkanRenderer::CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, UINT uiMipLevel)
{
	VkImageViewCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	createInfo.image = image;
	createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	createInfo.format = format;
	createInfo.subresourceRange.aspectMask = aspectFlags;
	createInfo.subresourceRange.baseMipLevel = 0;
	createInfo.subresourceRange.levelCount = uiMipLevel;
	createInfo.subresourceRange.baseArrayLayer = 0;
	createInfo.subresourceRange.layerCount = 1;

	VkImageView imageView;
	VULKAN_ASSERT(vkCreateImageView(m_LogicalDevice, &createInfo, nullptr, &imageView), "Create image view failed");

	return imageView;
}

void VulkanRenderer::CreateSwapChainImages()
{
	UINT uiImageCount = 0;
	vkGetSwapchainImagesKHR(m_LogicalDevice, m_SwapChain, &uiImageCount, nullptr);
	ASSERT(uiImageCount > 0, "Find no image in swap chain");
	m_vecSwapChainImages.resize(uiImageCount);
	vkGetSwapchainImagesKHR(m_LogicalDevice, m_SwapChain, &uiImageCount, m_vecSwapChainImages.data());
}

void VulkanRenderer::CreateSwapChainImageViews()
{
	m_vecSwapChainImageViews.resize(m_vecSwapChainImages.size());
	for (UINT i = 0; i < m_vecSwapChainImageViews.size(); ++i)
	{
		m_vecSwapChainImageViews[i] = CreateImageView(m_vecSwapChainImages[i], m_SwapChainFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
	}
}

void VulkanRenderer::CreateSwapChainFrameBuffers()
{
	m_DepthFormat = ChooseDepthFormat(false);

	CreateImageAndBindMemory(m_SwapChainExtent2D.width, m_SwapChainExtent2D.height, 
		1, VK_SAMPLE_COUNT_1_BIT, 
		m_DepthFormat,
		VK_IMAGE_TILING_OPTIMAL, 
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		m_DepthImage, m_DepthImageMemory);

	m_DepthImageView = CreateImageView(m_DepthImage, m_DepthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);

	ChangeImageLayout(m_DepthImage, m_DepthFormat, 1,
		VK_IMAGE_LAYOUT_UNDEFINED, 
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

	m_vecSwapChainFrameBuffers.resize(m_vecSwapChainImageViews.size());
	for (size_t i = 0; i < m_vecSwapChainImageViews.size(); ++i)
	{
		std::vector<VkImageView> vecImageViewAttachments = {
			m_vecSwapChainImageViews[i], //潜规则：depth不能是第一个
			m_DepthImageView,
		};

		VkFramebufferCreateInfo frameBufferCreateInfo{};
		frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		frameBufferCreateInfo.renderPass = m_RenderPass;
		frameBufferCreateInfo.attachmentCount = static_cast<uint32_t>(vecImageViewAttachments.size());
		frameBufferCreateInfo.pAttachments = vecImageViewAttachments.data();
		frameBufferCreateInfo.width = m_SwapChainExtent2D.width;
		frameBufferCreateInfo.height = m_SwapChainExtent2D.height;
		frameBufferCreateInfo.layers = 1;

		VULKAN_ASSERT(vkCreateFramebuffer(m_LogicalDevice, &frameBufferCreateInfo, nullptr, &m_vecSwapChainFrameBuffers[i]), "Create frame buffer failed");
	}
}

VkFormat VulkanRenderer::ChooseDepthFormat(bool bCheckSamplingSupport)
{
	//后两种格式包含Stencil Component
	std::vector<VkFormat> vecFormats = {
		VK_FORMAT_D32_SFLOAT_S8_UINT,
		VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D16_UNORM_S8_UINT,
		VK_FORMAT_D16_UNORM
	};

	for (const auto& format : vecFormats)
	{
		VkFormatProperties formatProperties;
		vkGetPhysicalDeviceFormatProperties(m_PhysicalDevice, format, &formatProperties);

		// Format must support depth stencil attachment for optimal tiling
		if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
		{
			if (bCheckSamplingSupport)
			{
				if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
					continue;
			}
			return format;
		}
	}

	ASSERT(false, "No support depth format");
	return VK_FORMAT_D32_SFLOAT_S8_UINT;
}

void VulkanRenderer::CreateRenderPass()
{
	std::array<VkAttachmentDescription, 2> attachmentDescriptions = {};

	//设置Color Attachment Description, Reference
	attachmentDescriptions[0].format = m_SwapChainFormat;
	attachmentDescriptions[0].samples = VK_SAMPLE_COUNT_1_BIT; //不使用多重采样
	attachmentDescriptions[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; //RenderPass开始前清除
	attachmentDescriptions[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE; //RenderPass结束后保留其内容用来present
	attachmentDescriptions[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescriptions[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	//attachmentDescriptions[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; //适用于present的布局
	attachmentDescriptions[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; //让UI的renderpass负责present

	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	//设置Depth Attachment Description, Reference
	attachmentDescriptions[1].format = ChooseDepthFormat(false);
	attachmentDescriptions[1].samples = VK_SAMPLE_COUNT_1_BIT; //不使用多重采样
	attachmentDescriptions[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; //RenderPass开始前清除
	attachmentDescriptions[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; //RenderPass结束后不再需要
	attachmentDescriptions[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescriptions[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachmentDescriptions[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; //适用于depth stencil的布局

	VkAttachmentReference depthAttachmentRef{};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpassDescription{};
	subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDescription.colorAttachmentCount = 1;
	subpassDescription.pColorAttachments = &colorAttachmentRef;
	subpassDescription.pDepthStencilAttachment = &depthAttachmentRef;

	std::array<VkSubpassDependency, 2> dependencies = {};

	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
	dependencies[0].dependencyFlags = 0;

	dependencies[1].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].dstSubpass = 0;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].srcAccessMask = 0;
	dependencies[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
	dependencies[1].dependencyFlags = 0;

	VkRenderPassCreateInfo renderPassCreateInfo = {};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = static_cast<UINT>(attachmentDescriptions.size());
	renderPassCreateInfo.pAttachments = attachmentDescriptions.data();
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpassDescription;
	renderPassCreateInfo.dependencyCount = static_cast<UINT>(dependencies.size());
	renderPassCreateInfo.pDependencies = dependencies.data();

	VULKAN_ASSERT(vkCreateRenderPass(m_LogicalDevice, &renderPassCreateInfo, nullptr, &m_RenderPass), "Create render pass failed");
}

void VulkanRenderer::CreateTransferCommandPool()
{
	const auto& physicalDeviceInfo = m_mapPhysicalDeviceInfo.at(m_PhysicalDevice);
	
	VkCommandPoolCreateInfo commandPoolCreateInfo{};
	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	commandPoolCreateInfo.queueFamilyIndex = physicalDeviceInfo.graphicFamilyIdx.value();

	VULKAN_ASSERT(vkCreateCommandPool(m_LogicalDevice, &commandPoolCreateInfo, nullptr, &m_TransferCommandPool), "Create transfer command pool failed");
}

VkCommandBuffer VulkanRenderer::BeginSingleTimeCommand()
{
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = m_TransferCommandPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer singleTimeCommandBuffer;
	VULKAN_ASSERT(vkAllocateCommandBuffers(m_LogicalDevice, &allocInfo, &singleTimeCommandBuffer), "Allocate single time command buffer failed");

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; //告诉驱动只提交一次，以更好优化

	vkBeginCommandBuffer(singleTimeCommandBuffer, &beginInfo);

	return singleTimeCommandBuffer;
}

void VulkanRenderer::EndSingleTimeCommand(VkCommandBuffer commandBuffer)
{
	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	//严格来说需要一个transferQueue，但是一般graphicQueue和presentQueue都带有transfer功能（Pick PhysicalDevice已确保一致）
	vkQueueSubmit(m_GraphicQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(m_GraphicQueue); //这里也可以使用Fence + vkWaitForFence，可以同步多个submit操作

	vkFreeCommandBuffers(m_LogicalDevice, m_TransferCommandPool, 1, &commandBuffer);
}

std::vector<char> VulkanRenderer::ReadShaderFile(const std::filesystem::path& filepath)
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

VkShaderModule VulkanRenderer::CreateShaderModule(const std::vector<char>& vecBytecode)
{
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = vecBytecode.size();
	createInfo.pCode = reinterpret_cast<const UINT*>(vecBytecode.data());

	VkShaderModule shaderModule;
	VULKAN_ASSERT(vkCreateShaderModule(m_LogicalDevice, &createInfo, nullptr, &shaderModule), "Create shader module failed");

	return shaderModule;
}

void VulkanRenderer::CreateShader()
{
	m_mapShaderModule.clear();

	ASSERT(m_mapShaderPath.size() > 0, "Detect no shader spv file");

	for (const auto& spvPath : m_mapShaderPath)
	{
		auto shaderModule = CreateShaderModule(ReadShaderFile(spvPath.second));

		m_mapShaderModule[spvPath.first] = shaderModule;
	}
}

UINT VulkanRenderer::FindSuitableMemoryTypeIndex(UINT typeFilter, VkMemoryPropertyFlags properties)
{
	const auto& memoryProperties = m_mapPhysicalDeviceInfo.at(m_PhysicalDevice).memoryProperties;

	for (UINT i = 0; i < memoryProperties.memoryTypeCount; ++i)
	{
		if (typeFilter & (1 << i))
		{
			if ((memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
				return i;
		}
	}

	ASSERT(false, "Find no suitable memory type");
	return 0;
}

void VulkanRenderer::AllocateBufferMemory(VkMemoryPropertyFlags propertyFlags, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
	//MemoryRequirements的参数如下：
	//memoryRequirements.size			所需内存的大小
	//memoryRequirements.alignment		所需内存的对齐方式，由Buffer的usage和flags参数决定
	//memoryRequirements.memoryTypeBits 适合该Buffer的内存类型（位值）

	VkMemoryRequirements memoryRequirements;
	vkGetBufferMemoryRequirements(m_LogicalDevice, buffer, &memoryRequirements);

	VkMemoryAllocateInfo memoryAllocInfo{};
	memoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocInfo.allocationSize = memoryRequirements.size;
	//显卡有不同类型的内存，不同类型的内存所允许的操作与效率各不相同，需要根据需求寻找最适合的内存类型
	memoryAllocInfo.memoryTypeIndex = FindSuitableMemoryTypeIndex(memoryRequirements.memoryTypeBits, propertyFlags);

	VULKAN_ASSERT(vkAllocateMemory(m_LogicalDevice, &memoryAllocInfo, nullptr, &bufferMemory), "Allocate buffer memory failed");
}

void VulkanRenderer::CreateBufferAndBindMemory(VkDeviceSize deviceSize, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags propertyFlags, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
	VkBufferCreateInfo BufferCreateInfo{};
	BufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	BufferCreateInfo.size = deviceSize;	//要创建的Buffer的大小
	BufferCreateInfo.usage = usageFlags;	//使用目的，比如用作VertexBuffer或IndexBuffer或其他
	BufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; //Buffer可以被多个QueueFamily共享，这里选择独占模式
	BufferCreateInfo.flags = 0;	//用来配置缓存的内存稀疏程度，0为默认值

	VULKAN_ASSERT(vkCreateBuffer(m_LogicalDevice, &BufferCreateInfo, nullptr, &buffer), "Create buffer failed");

	AllocateBufferMemory(propertyFlags, buffer, bufferMemory);

	vkBindBufferMemory(m_LogicalDevice, buffer, bufferMemory, 0);
}


// Wrapper functions for aligned memory allocation
// There is currently no standard for this in C++ that works across all platforms and vendors, so we abstract this
void* alignedAlloc(size_t size, size_t alignment)
{
	void* data = nullptr;
#if defined(_MSC_VER) || defined(__MINGW32__)
	data = _aligned_malloc(size, alignment);
#else
	int res = posix_memalign(&data, alignment, size);
	if (res != 0)
		data = nullptr;
#endif
	return data;
}

void alignedFree(void* data)
{
#if	defined(_MSC_VER) || defined(__MINGW32__)
	_aligned_free(data);
#else
	free(data);
#endif
}

void VulkanRenderer::CreateUniformBuffers()
{
	auto physicalDeviceProperties = m_mapPhysicalDeviceInfo.at(m_PhysicalDevice).properties;
	size_t minUboAlignment = physicalDeviceProperties.limits.minUniformBufferOffsetAlignment;
	m_DynamicAlignment = sizeof(glm::mat4); //model + view + proj
	if (minUboAlignment > 0)
	{
		m_DynamicAlignment = (m_DynamicAlignment + minUboAlignment - 1) & ~(minUboAlignment - 1);
	}

	VkDeviceSize dynamicUniformBufferSize = m_DynamicAlignment * INSTANCE_NUM;

	m_DynamicUBOData.model = (glm::mat4*)alignedAlloc(sizeof(glm::mat4), m_DynamicAlignment);
	m_DynamicUBOData.view = (glm::mat4*)alignedAlloc(sizeof(glm::mat4), m_DynamicAlignment);
	m_DynamicUBOData.proj = (glm::mat4*)alignedAlloc(sizeof(glm::mat4), m_DynamicAlignment);

	m_vecDynamicUniformBuffers.resize(m_vecSwapChainImages.size());
	m_vecDynamicUniformBufferMemories.resize(m_vecSwapChainImages.size());

	//为并行渲染的每一帧图像创建独立的Uniform Buffer
	for (size_t i = 0; i < m_vecSwapChainImages.size(); ++i)
	{
		CreateBufferAndBindMemory(dynamicUniformBufferSize, 
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			m_vecDynamicUniformBuffers[i], 
			m_vecDynamicUniformBufferMemories[i]
		);
	}
}

void VulkanRenderer::CreateTextureSampler()
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
	auto& physicalDeviceProperties = m_mapPhysicalDeviceInfo.at(m_PhysicalDevice).properties;
	createInfo.maxAnisotropy = physicalDeviceProperties.limits.maxSamplerAnisotropy;

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
	createInfo.maxLod = 0.f;

	VULKAN_ASSERT(vkCreateSampler(m_LogicalDevice, &createInfo, nullptr, &m_TextureSampler), "Create texture sampler failed");
}

void VulkanRenderer::AllocateImageMemory(VkMemoryPropertyFlags propertyFlags, VkImage& image, VkDeviceMemory& imageMemory)
{
	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(m_LogicalDevice, image, &memoryRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memoryRequirements.size;
	allocInfo.memoryTypeIndex = FindSuitableMemoryTypeIndex(memoryRequirements.memoryTypeBits, propertyFlags);

	VULKAN_ASSERT(vkAllocateMemory(m_LogicalDevice, &allocInfo, nullptr, &imageMemory), "Allocate image memory failed");
}

void VulkanRenderer::CreateImageAndBindMemory(uint32_t uiWidth, uint32_t uiHeight, uint32_t uiMipLevel, VkSampleCountFlagBits sampleCount, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags propertyFlags, VkImage& image, VkDeviceMemory& imageMemory)
{
	VkImageCreateInfo imageCreateInfo{};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.extent.width = static_cast<uint32_t>(uiWidth);
	imageCreateInfo.extent.height = static_cast<uint32_t>(uiHeight);
	imageCreateInfo.extent.depth = 1;
	imageCreateInfo.mipLevels = uiMipLevel;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.format = format;
	imageCreateInfo.tiling = tiling;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCreateInfo.usage = usage;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.samples = sampleCount;
	imageCreateInfo.flags = 0;

	VULKAN_ASSERT(vkCreateImage(m_LogicalDevice, &imageCreateInfo, nullptr, &image), "Create image failed");

	AllocateImageMemory(propertyFlags, image, imageMemory);

	vkBindImageMemory(m_LogicalDevice, image, imageMemory, 0);
}

bool VulkanRenderer::CheckFormatHasStencilComponent(VkFormat format)
{
	if (format == VK_FORMAT_S8_UINT
		|| format == VK_FORMAT_D16_UNORM_S8_UINT
		|| format == VK_FORMAT_D24_UNORM_S8_UINT
		|| format == VK_FORMAT_D32_SFLOAT_S8_UINT)
		return true;

	return false;
}

void VulkanRenderer::ChangeImageLayout(VkImage image, VkFormat format, uint32_t uiMipLevel, VkImageLayout oldLayout, VkImageLayout newLayout)
{
	VkCommandBuffer singleTimeCommandBuffer = BeginSingleTimeCommand();

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	//用于图像布局的转换
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	//用于传递Queue Family的所有权，不使用时设为IGNORED
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	//指定目标图像及作用范围
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = uiMipLevel;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	//指定barrier之前必须发生的资源操作类型，和barrier之后必须等待的资源操作类型
	//需要根据oldLayout和newLayout的类型来决定
	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = 0;

	if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		if (CheckFormatHasStencilComponent(format))
		{
			barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
	}
	else
	{
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	}

	VkPipelineStageFlags srcStage;
	VkPipelineStageFlags dstStage;
	if ((oldLayout == VK_IMAGE_LAYOUT_UNDEFINED) && (newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL))
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; //传输的写入操作不需要等待任何对象，因此指定一个最早出现的管线阶段
		dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT; //传输的写入操作必须在传输阶段进行（伪阶段，表示传输发生）
	}
	else if ((oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) && (newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
	{
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else if ((oldLayout == VK_IMAGE_LAYOUT_UNDEFINED) && (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL))
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
			| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	}
	else
	{
		ASSERT(false, "Unsupport image layout change type");
	}

	vkCmdPipelineBarrier(singleTimeCommandBuffer,
		srcStage,		//发生在barrier之前的管线阶段
		dstStage,		//发生在barrier之后的管线阶段
		0,				//若设置为VK_DEPENDENCY_BY_REGION_BIT，则允许按区域部分读取资源
		0, nullptr,		//Memory Barrier及数量
		0, nullptr,		//Buffer Memory Barrier及数量
		1, &barrier);	//Image Memory Barrier及数量

	EndSingleTimeCommand(singleTimeCommandBuffer);
}

void VulkanRenderer::TransferImageDataByStageBuffer(void* pData, VkDeviceSize imageSize, VkImage& image, UINT uiWidth, UINT uiHeight)
{
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	CreateBufferAndBindMemory(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer, stagingBufferMemory);

	void* imageData;
	vkMapMemory(m_LogicalDevice, stagingBufferMemory, 0, imageSize, 0, &imageData);
	memcpy(imageData, pData, static_cast<size_t>(imageSize));
	vkUnmapMemory(m_LogicalDevice, stagingBufferMemory);

	VkCommandBuffer singleTimeCommandBuffer = BeginSingleTimeCommand();

	VkBufferImageCopy region{};
	//指定要复制的数据在buffer中的偏移量
	region.bufferOffset = 0;
	//指定数据在memory中的存放方式，用于对齐
	//若都为0，则数据在memory中会紧凑存放
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	//指定数据被复制到image的哪一部分
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = { uiWidth, uiHeight, 1 };
	vkCmdCopyBufferToImage(singleTimeCommandBuffer, stagingBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	EndSingleTimeCommand(singleTimeCommandBuffer);

	vkDestroyBuffer(m_LogicalDevice, stagingBuffer, nullptr);
	vkFreeMemory(m_LogicalDevice, stagingBufferMemory, nullptr);
}

void VulkanRenderer::CreateTextureImageAndFillData()
{
	int nTexWidth = 0;
	int nTexHeight = 0;
	int nTexChannel = 0;
	stbi_uc* pixels = stbi_load(m_TexturePath.string().c_str(), &nTexWidth, &nTexHeight, &nTexChannel, STBI_rgb_alpha);
	ASSERT(pixels, std::format("Stb load image {} failed", m_TexturePath.string()));

	VkDeviceSize imageSize = (uint64_t)nTexWidth * (uint64_t)nTexHeight * 4;

	CreateImageAndBindMemory(nTexWidth, nTexHeight, m_uiMipmapLevel,
		VK_SAMPLE_COUNT_1_BIT,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		m_TextureImage, m_TextureImageMemory);

	//copy之前，将layout从初始的undefined转为transfer dst
	ChangeImageLayout(m_TextureImage,
		VK_FORMAT_R8G8B8A8_SRGB,				//image format
		m_uiMipmapLevel,						//mipmap level
		VK_IMAGE_LAYOUT_UNDEFINED,				//src layout
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);	//dst layout

	TransferImageDataByStageBuffer(pixels, imageSize, m_TextureImage, static_cast<UINT>(nTexWidth), static_cast<UINT>(nTexHeight));

	//transfer之后，将layout从transfer dst转为shader readonly
	//如果使用mipmap，之后在generateMipmaps中将layout转为shader readonly

	ChangeImageLayout(m_TextureImage,
		VK_FORMAT_R8G8B8A8_SRGB, 
		m_uiMipmapLevel,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	//generateMipmaps(m_TextureImage, VK_FORMAT_R8G8B8A8_SRGB, nTexWidth, nTexHeight, m_uiMipmapLevel);

	stbi_image_free(pixels);
}

void VulkanRenderer::CreateTextureImageView()
{
	m_TextureImageView = CreateImageView(m_TextureImage,
		VK_FORMAT_R8G8B8A8_SRGB,	//格式为sRGB
		VK_IMAGE_ASPECT_COLOR_BIT,	//aspectFlags为COLOR_BIT
		m_uiMipmapLevel
	);
}

void VulkanRenderer::CreateDescriptorSetLayout()
{
	//UniformBufferObject Binding
	VkDescriptorSetLayoutBinding uboLayoutBinding{};
	uboLayoutBinding.binding = 0; //对应Vertex Shader中的layout(binding=0)
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;	//类型为Dynamic Uniform Buffer
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; //只需要在vertex stage生效
	uboLayoutBinding.pImmutableSamplers = nullptr;

	//CombinedImageSampler Binding
	VkDescriptorSetLayoutBinding samplerLayoutBinding{};
	samplerLayoutBinding.binding = 1; ////对应Fragment Shader中的layout(binding=1)
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; //只用于fragment stage
	samplerLayoutBinding.pImmutableSamplers = nullptr;

	std::vector<VkDescriptorSetLayoutBinding> vecDescriptorLayoutBinding = {
		uboLayoutBinding,
		samplerLayoutBinding,
	};

	VkDescriptorSetLayoutCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	createInfo.bindingCount = static_cast<UINT>(vecDescriptorLayoutBinding.size());
	createInfo.pBindings = vecDescriptorLayoutBinding.data();

	VULKAN_ASSERT(vkCreateDescriptorSetLayout(m_LogicalDevice, &createInfo, nullptr, &m_DescriptorSetLayout), "Create descriptor layout failed");
}

void VulkanRenderer::CreateDescriptorPool()
{
	//ubo
	VkDescriptorPoolSize uboPoolSize{};
	uboPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	uboPoolSize.descriptorCount = static_cast<UINT>(m_vecSwapChainImages.size());

	//sampler
	VkDescriptorPoolSize samplerPoolSize{};
	samplerPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerPoolSize.descriptorCount = static_cast<UINT>(m_vecSwapChainImages.size());

	std::vector<VkDescriptorPoolSize> vecPoolSize = {
		uboPoolSize,
		samplerPoolSize,
	};

	VkDescriptorPoolCreateInfo poolCreateInfo{};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCreateInfo.poolSizeCount = static_cast<UINT>(vecPoolSize.size());
	poolCreateInfo.pPoolSizes = vecPoolSize.data();
	poolCreateInfo.maxSets = static_cast<UINT>(m_vecSwapChainImages.size());

	VULKAN_ASSERT(vkCreateDescriptorPool(m_LogicalDevice, &poolCreateInfo, nullptr, &m_DescriptorPool), "Create descriptor pool failed");
}

void VulkanRenderer::CreateDescriptorSets()
{
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorSetCount = static_cast<UINT>(m_vecSwapChainImages.size());
	allocInfo.descriptorPool = m_DescriptorPool;

	std::vector<VkDescriptorSetLayout> vecDupDescriptorSetLayout(m_vecSwapChainImages.size(), m_DescriptorSetLayout);
	allocInfo.pSetLayouts = vecDupDescriptorSetLayout.data();

	m_vecDescriptorSets.resize(m_vecSwapChainImages.size());
	VULKAN_ASSERT(vkAllocateDescriptorSets(m_LogicalDevice, &allocInfo, m_vecDescriptorSets.data()), "Allocate desctiprot sets failed");

	for (size_t i = 0; i < m_vecSwapChainImages.size(); ++i)
	{
		//ubo
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = m_vecDynamicUniformBuffers[i];
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(UniformBufferObject);

		VkWriteDescriptorSet uboWrite{};
		uboWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		uboWrite.dstSet = m_vecDescriptorSets[i];
		uboWrite.dstBinding = 0;
		uboWrite.dstArrayElement = 0;
		uboWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		uboWrite.descriptorCount = 1;
		uboWrite.pBufferInfo = &bufferInfo;

		//sampler
		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = m_TextureImageView;
		imageInfo.sampler = m_TextureSampler;

		VkWriteDescriptorSet samplerWrite{};
		samplerWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		samplerWrite.dstSet = m_vecDescriptorSets[i];
		samplerWrite.dstBinding = 1;
		samplerWrite.dstArrayElement = 0;
		samplerWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerWrite.descriptorCount = 1;
		samplerWrite.pImageInfo = &imageInfo;

		std::vector<VkWriteDescriptorSet> vecDescriptorWrite = {
			uboWrite,
			samplerWrite,
		};

		vkUpdateDescriptorSets(m_LogicalDevice, static_cast<UINT>(vecDescriptorWrite.size()), vecDescriptorWrite.data(), 0, nullptr);
	}
}

void VulkanRenderer::TransferBufferDataByStageBuffer(void* pData, VkDeviceSize bufferSize, VkBuffer& buffer)
{
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	CreateBufferAndBindMemory(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer, stagingBufferMemory);

	void* imageData;
	vkMapMemory(m_LogicalDevice, stagingBufferMemory, 0, bufferSize, 0, &imageData);
	memcpy(imageData, pData, static_cast<size_t>(bufferSize));
	vkUnmapMemory(m_LogicalDevice, stagingBufferMemory);

	VkCommandBuffer singleTimeCommandBuffer = BeginSingleTimeCommand();

	VkBufferCopy copyRegion{};
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = bufferSize;
	vkCmdCopyBuffer(singleTimeCommandBuffer, stagingBuffer, buffer, 1, &copyRegion);

	EndSingleTimeCommand(singleTimeCommandBuffer);

	vkDestroyBuffer(m_LogicalDevice, stagingBuffer, nullptr);
	vkFreeMemory(m_LogicalDevice, stagingBufferMemory, nullptr);
}

void VulkanRenderer::CreateVertexBuffer()
{
	Log::Info(std::format("Vertex count : {}", m_Vertices.size()));

	ASSERT(m_Vertices.size() > 0, "Vertex data empty");

	VkDeviceSize verticesSize = sizeof(m_Vertices[0]) * m_Vertices.size();

	//创建device local的vertexBufferMemory作为transfer的dst
	CreateBufferAndBindMemory(verticesSize,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, //用途是作为transfer的dst、以及vertexBuffer
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,	//具有性能最佳的DeviceLocal显存类型
		m_VertexBuffer, m_VertexBufferMemory);

	TransferBufferDataByStageBuffer(m_Vertices.data(), verticesSize, m_VertexBuffer);
}

void VulkanRenderer::CreateIndexBuffer()
{
	Log::Info(std::format("Index count : {}", m_Indices.size()));

	if (m_Indices.size() == 0)
		return;

	VkDeviceSize indicesSize = sizeof(m_Indices[0]) * m_Indices.size();

	CreateBufferAndBindMemory(indicesSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		m_IndexBuffer, m_IndexBufferMemory);

	TransferBufferDataByStageBuffer(m_Indices.data(), indicesSize, m_IndexBuffer);
}

void VulkanRenderer::CreateCommandPool()
{
	const auto& physicalDeviceInfo = m_mapPhysicalDeviceInfo.at(m_PhysicalDevice);

	VkCommandPoolCreateInfo commandPoolCreateInfo{};
	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	commandPoolCreateInfo.queueFamilyIndex = physicalDeviceInfo.graphicFamilyIdx.value();

	VULKAN_ASSERT(vkCreateCommandPool(m_LogicalDevice, &commandPoolCreateInfo, nullptr, &m_CommandPool), "Create command pool failed");
}

void VulkanRenderer::CreateCommandBuffer()
{
	m_vecCommandBuffers.resize(m_vecSwapChainImages.size());

	VkCommandBufferAllocateInfo commandBufferAllocator{};
	commandBufferAllocator.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocator.commandPool = m_CommandPool;
	commandBufferAllocator.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocator.commandBufferCount = static_cast<UINT>(m_vecCommandBuffers.size());

	VULKAN_ASSERT(vkAllocateCommandBuffers(m_LogicalDevice, &commandBufferAllocator, m_vecCommandBuffers.data()), "Allocate command buffer failed");
}

void VulkanRenderer::CreateGraphicPipeline()
{
	/****************************可编程管线*******************************/

	ASSERT(m_mapShaderModule.find(VK_SHADER_STAGE_VERTEX_BIT) != m_mapShaderModule.end(), "No vertex shader module");
	ASSERT(m_mapShaderModule.find(VK_SHADER_STAGE_FRAGMENT_BIT) != m_mapShaderModule.end(), "No fragment shader module");

	VkPipelineShaderStageCreateInfo vertShaderStageCreateInfo{};
	vertShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageCreateInfo.module = m_mapShaderModule.at(VK_SHADER_STAGE_VERTEX_BIT); //Bytecode
	vertShaderStageCreateInfo.pName = "main"; //要invoke的函数

	VkPipelineShaderStageCreateInfo fragShaderStageCreateInfo{};
	fragShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageCreateInfo.module = m_mapShaderModule.at(VK_SHADER_STAGE_FRAGMENT_BIT); //Bytecode
	fragShaderStageCreateInfo.pName = "main"; //要invoke的函数

	VkPipelineShaderStageCreateInfo shaderStageCreateInfos[] = {
		vertShaderStageCreateInfo,
		fragShaderStageCreateInfo,
	};

	/*****************************固定管线*******************************/

	//-----------------------Dynamic State--------------------------//
	//一般会将Viewport和Scissor设为dynamic，以方便随时修改
	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo{};
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	if (m_bViewportAndScissorIsDynamic)
	{
		std::vector<VkDynamicState> vecDynamicStates = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR,
		};
		dynamicStateCreateInfo.dynamicStateCount = static_cast<UINT>(vecDynamicStates.size());
		dynamicStateCreateInfo.pDynamicStates = vecDynamicStates.data();
	}

	//-----------------------Vertex Input State--------------------------//
	auto bindingDescription = Vertex3D::GetBindingDescription();
	auto attributeDescriptions = Vertex3D::GetAttributeDescriptions();
	VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo{};
	vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
	vertexInputCreateInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputCreateInfo.vertexAttributeDescriptionCount = static_cast<UINT>(attributeDescriptions.size());
	vertexInputCreateInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	//-----------------------Input Assembly State------------------------//
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo{};
	inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;


	//-----------------------Viewport State--------------------------//
	VkPipelineViewportStateCreateInfo viewportStateCreateInfo{};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.scissorCount = 1;
	//如果没有把Viewport和Scissor设为dynamic state，则需要在此处指定（使用这种设置方法，设置的Viewport是不可变的）
	if (!m_bViewportAndScissorIsDynamic)
	{
		//设置Viewport，范围为[0,0]到[width,height]
		VkViewport viewport{};
		viewport.x = 0.f;
		viewport.y = 0.f;
		viewport.width = static_cast<float>(m_SwapChainExtent2D.width);
		viewport.height = static_cast<float>(m_SwapChainExtent2D.height);
		viewport.minDepth = 0.f;
		viewport.maxDepth = 1.f;

		//设置裁剪区域Scissor Rectangle
		VkRect2D scissor{};
		scissor.offset = { 0,0 };
		VkExtent2D ScissorExtent;
		ScissorExtent.width = m_SwapChainExtent2D.width;
		ScissorExtent.height = m_SwapChainExtent2D.height;
		scissor.extent = ScissorExtent;

		viewportStateCreateInfo.pViewports = &viewport;
		viewportStateCreateInfo.pScissors = &scissor;
	}

	//-----------------------Raserization State--------------------------//
	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo{};
	rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;	//开启后，超过远近平面的部分会被截断在远近平面上，而不是丢弃
	rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;	//开启后，禁止所有图元经过光栅化器
	rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;	//图元模式，可以是FILL、LINE、POINT
	rasterizationStateCreateInfo.lineWidth = 1.f;	//指定光栅化后的线段宽度
	rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE;	//剔除模式，可以是NONE、FRONT、BACK、FRONT_AND_BACK
	rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; //顶点序，可以是顺时针cw或逆时针ccw
	rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE; //深度偏移，一般用于Shaodw Map中避免阴影痤疮
	rasterizationStateCreateInfo.depthBiasConstantFactor = 0.f;
	rasterizationStateCreateInfo.depthBiasClamp = 0.f;
	rasterizationStateCreateInfo.depthBiasSlopeFactor = 0.f;

	//-----------------------Multisample State--------------------------//
	VkPipelineMultisampleStateCreateInfo multisamplingStateCreateInfo{};
	multisamplingStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisamplingStateCreateInfo.sampleShadingEnable = (VkBool32)(m_uiMipmapLevel > 1);
	multisamplingStateCreateInfo.minSampleShading = 0.8f;
	multisamplingStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisamplingStateCreateInfo.minSampleShading = 1.f;
	multisamplingStateCreateInfo.pSampleMask = nullptr;
	multisamplingStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
	multisamplingStateCreateInfo.alphaToOneEnable = VK_FALSE;

	//-----------------------Depth Stencil State--------------------------//
	VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo{};
	depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilStateCreateInfo.depthTestEnable = VK_TRUE;
	depthStencilStateCreateInfo.depthWriteEnable = VK_TRUE;
	depthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.minDepthBounds = 0.f;
	depthStencilStateCreateInfo.maxDepthBounds = 1.f;
	depthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.front = {};
	depthStencilStateCreateInfo.back = {};

	//-----------------------Color Blend State--------------------------//
	VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo{};
	colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
	colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
	colorBlendStateCreateInfo.attachmentCount = 1;

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT
		| VK_COLOR_COMPONENT_G_BIT
		| VK_COLOR_COMPONENT_B_BIT
		| VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	colorBlendStateCreateInfo.pAttachments = &colorBlendAttachment;
	colorBlendStateCreateInfo.blendConstants[0] = 0.f;
	colorBlendStateCreateInfo.blendConstants[1] = 0.f;
	colorBlendStateCreateInfo.blendConstants[2] = 0.f;
	colorBlendStateCreateInfo.blendConstants[3] = 0.f;

	//-----------------------Pipeline Layout--------------------------//
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &m_DescriptorSetLayout;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

	VULKAN_ASSERT(vkCreatePipelineLayout(m_LogicalDevice, &pipelineLayoutCreateInfo, nullptr, &m_GraphicPipelineLayout), "Create pipeline layout failed");

	/***********************************************************************/
	VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.stageCount = 2;
	pipelineCreateInfo.pStages = shaderStageCreateInfos;
	pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
	pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
	pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	pipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
	pipelineCreateInfo.pMultisampleState = &multisamplingStateCreateInfo;
	pipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
	pipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
	pipelineCreateInfo.layout = m_GraphicPipelineLayout;
	pipelineCreateInfo.renderPass = m_RenderPass;
	pipelineCreateInfo.subpass = 0;
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineCreateInfo.basePipelineIndex = -1;

	VULKAN_ASSERT(vkCreateGraphicsPipelines(m_LogicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &m_GraphicPipeline), "Create graphic pipeline failed");
}

void VulkanRenderer::CreateSyncObjects()
{
	m_vecImageAvailableSemaphores.resize(m_vecSwapChainImages.size());
	m_vecRenderFinishedSemaphores.resize(m_vecSwapChainImages.size());
	m_vecInFlightFences.resize(m_vecSwapChainImages.size());

	VkSemaphoreCreateInfo semaphoreCreateInfo{};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceCreateInfo{};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; //初值为signaled

	for (int i = 0; i < m_vecSwapChainImages.size(); ++i)
	{
		VULKAN_ASSERT(vkCreateSemaphore(m_LogicalDevice, &semaphoreCreateInfo, nullptr, &m_vecImageAvailableSemaphores[i]), "Create image available semaphore failed");
		VULKAN_ASSERT(vkCreateSemaphore(m_LogicalDevice, &semaphoreCreateInfo, nullptr, &m_vecRenderFinishedSemaphores[i]), "Create render finished semaphore failed");
		VULKAN_ASSERT(vkCreateFence(m_LogicalDevice, &fenceCreateInfo, nullptr, &m_vecInFlightFences[i]), "Create inflight fence failed");
	}
}

void VulkanRenderer::SetupCamera()
{
	m_Camera.Set(45.f, (float)m_SwapChainExtent2D.width / (float)m_SwapChainExtent2D.height, 0.1f, 100.f);
	m_Camera.SetViewportSize(static_cast<float>(m_SwapChainExtent2D.width), static_cast<float>(m_SwapChainExtent2D.height));
	m_Camera.SetWindow(m_pWindow);

	glfwSetWindowUserPointer(m_pWindow, (void*)&m_Camera);

	glfwSetScrollCallback(m_pWindow, [](GLFWwindow* window, double dOffsetX, double dOffsetY)
		{
			if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
				return;
			auto& camera = *(Camera*)glfwGetWindowUserPointer(window);
			camera.OnMouseScroll(dOffsetX, dOffsetY);
		}
	);
}

void VulkanRenderer::RecordCommandBuffer(VkCommandBuffer& commandBuffer, UINT uiIdx)
{
	VkCommandBufferBeginInfo commandBufferBeginInfo{};
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBufferBeginInfo.flags = 0;
	commandBufferBeginInfo.pInheritanceInfo = nullptr;

	VULKAN_ASSERT(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo), "Begin command buffer failed");

	VkRenderPassBeginInfo renderPassBeginInfo{};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = m_RenderPass;
	renderPassBeginInfo.framebuffer = m_vecSwapChainFrameBuffers[uiIdx];
	renderPassBeginInfo.renderArea.offset = { 0, 0 };
	renderPassBeginInfo.renderArea.extent = m_SwapChainExtent2D;
	std::array<VkClearValue, 2> aryClearColor;
	aryClearColor[0].color = { 0.f, 0.f, 0.f, 1.f };
	aryClearColor[1].depthStencil = { 1.f, 0 };
	renderPassBeginInfo.clearValueCount = static_cast<UINT>(aryClearColor.size());
	renderPassBeginInfo.pClearValues = aryClearColor.data();

	vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicPipeline);

	if (m_bViewportAndScissorIsDynamic)
	{
		VkViewport viewport{};
		viewport.x = 0.f;
		viewport.y = 0.f;
		viewport.width = static_cast<float>(m_SwapChainExtent2D.width);
		viewport.height = static_cast<float>(m_SwapChainExtent2D.height);
		viewport.minDepth = 0.f;
		viewport.maxDepth = 1.f;
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = m_SwapChainExtent2D;
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
	}

	VkBuffer vertexBuffers[] = {
		m_VertexBuffer,
	};
	VkDeviceSize offsets[]{ 0 };
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

	vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer, 0, VK_INDEX_TYPE_UINT32);

	for (UINT i = 0; i < INSTANCE_NUM; ++i)
	{
		UINT uiDynamicOffset = i * static_cast<UINT>(m_DynamicAlignment);

		vkCmdBindDescriptorSets(commandBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS, //descriptorSet并非Pipeline独有，因此需要指定是用于Graphic Pipeline还是Compute Pipeline
			m_GraphicPipelineLayout, //PipelineLayout中指定了descriptorSetLayout
			0,	//descriptorSet数组中第一个元素的下标 
			1,	//descriptorSet数组中元素的个数
			&m_vecDescriptorSets[uiIdx],
			1, //启用动态Uniform偏移
			&uiDynamicOffset	//指定动态Uniform的偏移
		);

		vkCmdDrawIndexed(commandBuffer, static_cast<UINT>(m_Indices.size()), 1, 0, 0, 0);
	}

	vkCmdEndRenderPass(commandBuffer);

	VULKAN_ASSERT(vkEndCommandBuffer(commandBuffer), "End command buffer failed");
}

void VulkanRenderer::UpdateUniformBuffer(UINT uiIdx)
{
	//static auto startTime = std::chrono::high_resolution_clock::now();
	//auto currentTime = std::chrono::high_resolution_clock::now();

	//float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	////time = 1.f;

	auto AxisX = glm::vec3(1.f, 0.f, 0.f);
	auto AxisY = glm::vec3(0.f, 1.f, 0.f);
	auto AxisZ = glm::vec3(0.f, 0.f, 1.f);

	UniformBufferObject ubo{};

	std::vector<glm::mat4> vecModelMat;
	for (UINT i = 0; i < INSTANCE_NUM; ++i)
	{
		float x = static_cast<float>(i);
		vecModelMat.push_back(glm::translate(glm::mat4(1.f), { x, 0.f, 0.f }));
	}

	std::vector<glm::mat4> vecViewMat;
	vecViewMat.resize(INSTANCE_NUM, m_Camera.GetViewMatrix());

	std::vector<glm::mat4> vecProjMat;
	vecProjMat.resize(INSTANCE_NUM, m_Camera.GetProjMatrix());

	ubo.model = vecModelMat.data();
	ubo.view = vecViewMat.data();
	ubo.proj = vecProjMat.data();
	//OpenGL与Vulkan的差异 - Y坐标是反的
	//ubo.proj[1][1] *= -1.f;

	//void* uniformBufferData;
	//vkMapMemory(m_LogicalDevice, m_vecDynamicUniformBufferMemories[uiIdx], 0, m_DynamicAlignment * INSTANCE_NUM, 0, &uniformBufferData);
	//memcpy(uniformBufferData, &ubo, m_DynamicAlignment * INSTANCE_NUM);
	//vkUnmapMemory(m_LogicalDevice, m_vecDynamicUniformBufferMemories[uiIdx]);

	void* uniformBufferData;
	vkMapMemory(m_LogicalDevice, m_vecDynamicUniformBufferMemories[uiIdx], 0, sizeof(ubo), 0, &uniformBufferData);
	memcpy(uniformBufferData, &ubo, sizeof(ubo));
	vkUnmapMemory(m_LogicalDevice, m_vecDynamicUniformBufferMemories[uiIdx]);
}

void VulkanRenderer::Render()
{
	m_uiFrameCounter++;

	if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) && !ImGui::IsAnyItemActive())
		m_Camera.Tick();

	g_UI.StartNewFrame();

	//等待fence的值变为signaled
	vkWaitForFences(m_LogicalDevice, 1, &m_vecInFlightFences[m_uiCurFrameIdx], VK_TRUE, UINT64_MAX);

	uint32_t uiImageIdx;
	VkResult res = vkAcquireNextImageKHR(m_LogicalDevice, m_SwapChain, UINT64_MAX,
		m_vecImageAvailableSemaphores[m_uiCurFrameIdx], VK_NULL_HANDLE, &uiImageIdx);
	if (res != VK_SUCCESS)
	{
		if (res == VK_ERROR_OUT_OF_DATE_KHR)
		{
			//SwapChain与WindowSurface不兼容，无法继续渲染
			//一般发生在window尺寸改变时
			RecreateSwapChain();
			return;
		}
		else if (res == VK_SUBOPTIMAL_KHR)
		{
			//SwapChain仍然可用，但是WindowSurface的properties不完全匹配
		}
		else
		{
			ASSERT(false, "Accquire next swap chain image failed");
		}
	}

	//重设fence为unsignaled
	vkResetFences(m_LogicalDevice, 1, &m_vecInFlightFences[m_uiCurFrameIdx]);

	//auto uiCommandBuffer = g_UI.FillCommandBuffer(*this, m_uiCurFrameIdx);

	vkResetCommandBuffer(m_vecCommandBuffers[m_uiCurFrameIdx], 0);

	RecordCommandBuffer(m_vecCommandBuffers[m_uiCurFrameIdx], uiImageIdx);

	auto uiCommandBuffer = g_UI.FillCommandBuffer(m_uiCurFrameIdx);

	UpdateUniformBuffer(m_uiCurFrameIdx);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = {
		m_vecImageAvailableSemaphores[m_uiCurFrameIdx],
	};
	VkPipelineStageFlags waitStages[] = {
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
	};
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;

	std::vector<VkCommandBuffer> commandBuffers = {
		m_vecCommandBuffers[m_uiCurFrameIdx],
		uiCommandBuffer,
	};
	submitInfo.commandBufferCount = static_cast<UINT>(commandBuffers.size());
	submitInfo.pCommandBuffers = commandBuffers.data();

	VkSemaphore signalSemaphore[] = {
		m_vecRenderFinishedSemaphores[m_uiCurFrameIdx],
	};
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphore;

	//submit之后，会将fence置为signaled
	VULKAN_ASSERT(vkQueueSubmit(m_GraphicQueue, 1, &submitInfo, m_vecInFlightFences[m_uiCurFrameIdx]), "Submit command buffer failed");

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphore;

	std::vector<VkSwapchainKHR> vecSwapChains = {
		m_SwapChain,
	};

	std::vector<UINT> vecImageIndices = {
		uiImageIdx,
	};
	presentInfo.swapchainCount = static_cast<UINT>(vecSwapChains.size());
	presentInfo.pSwapchains = vecSwapChains.data();
	presentInfo.pImageIndices = vecImageIndices.data();
	presentInfo.pResults = nullptr;

	res = vkQueuePresentKHR(m_PresentQueue, &presentInfo);
	if (res != VK_SUCCESS)
	{
		if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR || m_bFrameBufferResized)
		{
			RecreateSwapChain();
			m_bFrameBufferResized = false;
		}
		else
		{
			throw std::runtime_error("Present Swap Chain Image To Queue Failed");
		}
	}

	m_uiCurFrameIdx = (m_uiCurFrameIdx + 1) % static_cast<UINT>(m_vecSwapChainImages.size());
}

void VulkanRenderer::RecreateSwapChain()
{
	//特殊处理窗口最小化的情况
	int nWidth = 0;
	int nHeight = 0;
	glfwGetFramebufferSize(m_pWindow, &nWidth, &nHeight);
	while (nWidth == 0 || nHeight == 0)
	{
		glfwGetFramebufferSize(m_pWindow, &nWidth, &nHeight);
		glfwWaitEvents();
	}

	//等待资源结束占用
	vkDeviceWaitIdle(m_LogicalDevice);

	vkDestroyImageView(m_LogicalDevice, m_DepthImageView, nullptr);
	vkDestroyImage(m_LogicalDevice, m_DepthImage, nullptr);
	vkFreeMemory(m_LogicalDevice, m_DepthImageMemory, nullptr);

	for (const auto& frameBuffer : m_vecSwapChainFrameBuffers)
	{
		vkDestroyFramebuffer(m_LogicalDevice, frameBuffer, nullptr);
	}

	for (const auto& imageView : m_vecSwapChainImageViews)
	{
		vkDestroyImageView(m_LogicalDevice, imageView, nullptr);
	}

	//for (const auto& image : m_vecSwapChainImages)
	//{
	//	vkDestroyImage(m_LogicalDevice, image, nullptr);
	//}

	//清理SwapChain
	vkDestroySwapchainKHR(m_LogicalDevice, m_SwapChain, nullptr);

	CreateSwapChain();
	CreateRenderPass();

	CreateSwapChainImages();
	CreateSwapChainImageViews();
	CreateSwapChainFrameBuffers();
	
	CreateGraphicPipeline();

}



