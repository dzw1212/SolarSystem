#include "Core.h"
#include "VulkanRenderer.h"
#include "VulkanUtils.h"
#include "Log.h"

#include <chrono>
#include <mutex>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"
#include "glm/detail/type_vec3.hpp"
#include "glm/vec4.hpp"
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
// #define TINYGLTF_NOEXCEPTION // optional. disable exception handling.

#include "tiny_gltf.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include "ktx.h"
#include "ktxvulkan.h"

#define INSTANCE_NUM 9

static bool ENABLE_GUI = true;

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
	//m_TexturePath = "./Assert/Texture/metalplate01_rgba.ktx";

	m_ModelPath = "./Assert/Model/viking_room.obj";

	m_bViewportAndScissorIsDynamic = false;

	m_uiCurFrameIdx = 0;

	m_uiFPS = 0;
	m_uiFrameCounter = 0;

	m_DynamicAlignment = 0;
	m_UboBufferSize = 0;
	m_DynamicUboBufferSize = 0;
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

	CreateDepthImage();
	CreateDepthImageView();

	CreateSwapChainImages();
	CreateSwapChainImageViews();
	CreateSwapChainFrameBuffers();

	CreateCommandPool();
	CreateCommandBuffer();
	
	CreateSyncObjects();

	LoadModel("./Assert/Model/sphere.obj", m_Model);
	LoadTexture("./Assert/Texture/solarsystem_array_rgba8.ktx", m_Texture);
	CreateShader();
	CreateUniformBuffers();
	CreateDescriptorSetLayout();
	CreateDescriptorPool();
	CreateDescriptorSets();
	CreateGraphicPipelineLayout();
	CreateGraphicPipeline();

	//Skybox
	LoadTexture("./Assert/Texture/Skybox/milkyway_cubemap.ktx", m_SkyboxTexture);
	LoadModel("./Assert/Model/Skybox/cube.gltf", m_SkyboxModel);
	CreateSkyboxShader();
	CreateSkyboxUniformBuffers();
	CreateSkyboxDescriptorSetLayout();
	CreateSkyboxDescriptorPool();
	CreateSkyboxDescriptorSets();
	CreateSkyboxGraphicPipelineLayout();
	CreateSkyboxGraphicPipeline();

	//Mesh Grid
	CreateMeshGridVertexBuffer();
	CreateMeshGridIndexBuffer();
	CreateMeshGridShader();
	CreateMeshGridUniformBuffers();
	CreateMeshGridDescriptorSetLayout();
	CreateMeshGridDescriptorPool();
	CreateMeshGridDescriptorSets();
	CreateMeshGridGraphicPipelineLayout();
	CreateMeshGridGraphicPipeline();

	SetupCamera();

	if (ENABLE_GUI)
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

	//�ȴ�GPU����ǰ������ִ����ɣ���Դδ��ռ��ʱ��������
	vkDeviceWaitIdle(m_LogicalDevice);
}

void VulkanRenderer::Clean()
{
	_aligned_free(m_DynamicUboData.model);

	if (ENABLE_GUI)
		g_UI.Clean();

	FreeTexture(m_Texture);
	FreeModel(m_Model);

	FreeTexture(m_SkyboxTexture);
	FreeModel(m_SkyboxModel);

	//Skybox
	vkDestroyDescriptorPool(m_LogicalDevice, m_SkyboxDescriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(m_LogicalDevice, m_SkyboxDescriptorSetLayout, nullptr);
	for (size_t i = 0; i < m_vecSwapChainImages.size(); ++i)
	{
		vkFreeMemory(m_LogicalDevice, m_vecSkyboxUniformBufferMemories[i], nullptr);
		vkDestroyBuffer(m_LogicalDevice, m_vecSkyboxUniformBuffers[i], nullptr);
	}

	for (const auto& shaderModule : m_mapSkyboxShaderModule)
	{
		vkDestroyShaderModule(m_LogicalDevice, shaderModule.second, nullptr);
	}

	vkDestroyPipeline(m_LogicalDevice, m_SkyboxGraphicPipeline, nullptr);
	vkDestroyPipelineLayout(m_LogicalDevice, m_SkyboxGraphicPipelineLayout, nullptr);

	//Mesh Grid
	vkDestroyDescriptorPool(m_LogicalDevice, m_MeshGridDescriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(m_LogicalDevice, m_MeshGridDescriptorSetLayout, nullptr);
	for (size_t i = 0; i < m_vecSwapChainImages.size(); ++i)
	{
		vkFreeMemory(m_LogicalDevice, m_vecMeshGridUniformBufferMemories[i], nullptr);
		vkDestroyBuffer(m_LogicalDevice, m_vecMeshGridUniformBuffers[i], nullptr);
	}

	for (const auto& shaderModule : m_mapMeshGridShaderModule)
	{
		vkDestroyShaderModule(m_LogicalDevice, shaderModule.second, nullptr);
	}

	vkDestroyPipeline(m_LogicalDevice, m_MeshGridGraphicPipeline, nullptr);
	vkDestroyPipelineLayout(m_LogicalDevice, m_MeshGridGraphicPipelineLayout, nullptr);

	vkDestroyBuffer(m_LogicalDevice, m_MeshGridVertexBuffer, nullptr);
	vkFreeMemory(m_LogicalDevice, m_MeshGridVertexBufferMemory, nullptr);

	vkDestroyBuffer(m_LogicalDevice, m_MeshGridIndexBuffer, nullptr);
	vkFreeMemory(m_LogicalDevice, m_MeshGridIndexBufferMemory, nullptr);

	//----------------------------------------------------------------------------


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

	//����SwapChain���Զ��ͷ����µ�Image
	vkDestroySwapchainKHR(m_LogicalDevice, m_SwapChain, nullptr);

	vkDestroyDescriptorPool(m_LogicalDevice, m_DescriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(m_LogicalDevice, m_DescriptorSetLayout, nullptr);
	for (size_t i = 0; i < m_vecSwapChainImages.size(); ++i)
	{
		vkFreeMemory(m_LogicalDevice, m_vecUniformBufferMemories[i], nullptr);
		vkDestroyBuffer(m_LogicalDevice, m_vecUniformBuffers[i], nullptr);

		vkFreeMemory(m_LogicalDevice, m_vecDynamicUniformBufferMemories[i], nullptr);
		vkDestroyBuffer(m_LogicalDevice, m_vecDynamicUniformBuffers[i], nullptr);
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
	//����Extension�ĺ���������ֱ��ʹ�ã���Ҫ�ȷ�װһ�㣬ʹ��vkGetInstanceProcAddr�жϸú����Ƿ����
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func)
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	else
		return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void VulkanRenderer::DestoryDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
{
	//����Extension�ĺ���������ֱ��ʹ�ã���Ҫ�ȷ�װһ�㣬ʹ��vkGetInstanceProcAddr�жϸú����Ƿ����
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

		info.strDeviceTypeName = DZW_VulkanUtils::GetPhysicalDeviceTypeName(info.properties.deviceType);

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

		//��ȡӲ��֧�ֵ�capability������Image���������޵���Ϣ
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, m_WindowSurface, &info.swapChainSupportInfo.capabilities);

		//��ȡӲ��֧�ֵ�Surface Format�б�
		UINT uiFormatCount = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_WindowSurface, &uiFormatCount, nullptr);
		if (uiFormatCount > 0)
		{
			info.swapChainSupportInfo.vecSurfaceFormats.resize(uiFormatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_WindowSurface, &uiFormatCount, info.swapChainSupportInfo.vecSurfaceFormats.data());
		}

		//��ȡӲ��֧�ֵ�Present Mode�б�
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

	//����Ƿ��Ƕ����Կ�
	if (deviceInfo.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		nScore += 1000;

	//���֧�ֵ����ͼ��ߴ�,Խ��Խ��
	nScore += deviceInfo.properties.limits.maxImageDimension2D;

	//����Ƿ�֧�ּ�����ɫ��
	if (!deviceInfo.features.geometryShader)
		return 0;

	//����Ƿ�֧��Graphic Family, Present Family��Indexһ��
	if (!deviceInfo.IsGraphicAndPresentQueueFamilySame())
		return 0;

	//����Ƿ�֧��ָ����Extension(Swap Chain��)
	bool bExtensionSupport = checkDeviceExtensionSupport(deviceInfo);
	if (!bExtensionSupport)
		return 0;

	////���Swap Chain�Ƿ�����Ҫ��
	//bool bSwapChainAdequate = false;
	//if (bExtensionSupport) //����ȷ��֧��SwapChain
	//{
	//	SwapChainSupportDetails details = querySwapChainSupport(device);
	//	bSwapChainAdequate = !details.vecSurfaceFormats.empty() && !details.vecPresentModes.empty();
	//}
	//if (!bSwapChainAdequate)
	//	return false;

	////����Ƿ�֧�ָ������Թ���
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
	queueCreateInfo.pQueuePriorities = &queuePriority; //Queue�����ȼ�����Χ[0.f, 1.f]������CommandBuffer��ִ��˳��
	vecQueueCreateInfo.push_back(queueCreateInfo);

	VkPhysicalDeviceFeatures deviceFeatures{};
	deviceFeatures.fillModeNonSolid = VK_TRUE;
	deviceFeatures.wideLines = VK_TRUE;
	//deviceFeatures.samplerAnisotropy = VK_TRUE; //���ø������Թ��ˣ������������
	//deviceFeatures.sampleRateShading = VK_TRUE;	//����Sample Rate Shaing������MSAA�����

	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.queueCreateInfoCount = static_cast<UINT>(vecQueueCreateInfo.size());
	createInfo.pQueueCreateInfos = vecQueueCreateInfo.data();
	createInfo.pEnabledFeatures = &deviceFeatures;
	createInfo.enabledExtensionCount = static_cast<UINT>(m_vecDeviceExtensions.size()); //ע�⣡�˴���extension�봴��Instanceʱ��ͬ
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

	//�ҵ�֧��sRGB��ʽ��format�����û���򷵻ص�һ��
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

	//�ҵ�֧��UNORM��ʽ��format�����û���򷵻ص�һ��
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

	//����ѡ��MAILBOX�����FIFO
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

	//��ѡ�Կ�ʱ��ȷ��GraphicFamily��PresentFamilyһ��
	createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	createInfo.queueFamilyIndexCount = 0;
	createInfo.pQueueFamilyIndices = nullptr;

	//������ν���Transform
	createInfo.preTransform = physicalDeviceInfo.swapChainSupportInfo.capabilities.currentTransform; //�����κ�Transform

	//�����Ƿ�����Alphaͨ��
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; //������Alphaͨ��

	//�����Ƿ������������޳�
	createInfo.clipped = VK_TRUE;

	//oldSwapChain����window resizeʱ
	createInfo.oldSwapchain = VK_NULL_HANDLE;

	VULKAN_ASSERT(vkCreateSwapchainKHR(m_LogicalDevice, &createInfo, nullptr, &m_SwapChain), "Create swap chain failed");
}

VkImageView VulkanRenderer::CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, UINT uiMipLevelCount, UINT uiLayerCount, UINT uiFaceCount)
{
	VkImageViewType imageViewType;
	if (uiFaceCount == 6 && uiLayerCount == 1)
		imageViewType = VK_IMAGE_VIEW_TYPE_CUBE;
	else if (uiLayerCount > 1)
		imageViewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	else
		imageViewType = VK_IMAGE_VIEW_TYPE_2D;

	VkImageViewCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	createInfo.image = image;
	createInfo.viewType = imageViewType;
	createInfo.format = format;
	createInfo.subresourceRange.aspectMask = aspectFlags;
	createInfo.subresourceRange.baseMipLevel = 0;
	createInfo.subresourceRange.levelCount = uiMipLevelCount;
	createInfo.subresourceRange.baseArrayLayer = 0;
	createInfo.subresourceRange.layerCount = (uiFaceCount == 6) ? uiFaceCount : uiLayerCount;

	VkImageView imageView;
	VULKAN_ASSERT(vkCreateImageView(m_LogicalDevice, &createInfo, nullptr, &imageView), "Create image view failed");

	return imageView;
}

void VulkanRenderer::CreateDepthImage()
{
	m_DepthFormat = ChooseDepthFormat(false);

	CreateImageAndBindMemory(m_SwapChainExtent2D.width, m_SwapChainExtent2D.height,
		1, 1, 1,
		VK_SAMPLE_COUNT_1_BIT,
		m_DepthFormat,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		m_DepthImage, m_DepthImageMemory);
}

void VulkanRenderer::CreateDepthImageView()
{
	m_DepthImageView = CreateImageView(m_DepthImage, m_DepthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1, 1, 1);

	ChangeImageLayout(m_DepthImage, m_DepthFormat, 
		1, 1, 1,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
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
		m_vecSwapChainImageViews[i] = CreateImageView(m_vecSwapChainImages[i], m_SwapChainFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, 1);
	}
}

void VulkanRenderer::CreateSwapChainFrameBuffers()
{
	m_vecSwapChainFrameBuffers.resize(m_vecSwapChainImageViews.size());
	for (size_t i = 0; i < m_vecSwapChainImageViews.size(); ++i)
	{
		std::vector<VkImageView> vecImageViewAttachments = {
			m_vecSwapChainImageViews[i], //Ǳ����depth�����ǵ�һ��
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
	//�����ָ�ʽ����Stencil Component
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

	//����Color Attachment Description, Reference
	attachmentDescriptions[0].format = m_SwapChainFormat;
	attachmentDescriptions[0].samples = VK_SAMPLE_COUNT_1_BIT; //��ʹ�ö��ز���
	attachmentDescriptions[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; //RenderPass��ʼǰ���
	attachmentDescriptions[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE; //RenderPass������������������present
	attachmentDescriptions[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescriptions[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachmentDescriptions[0].finalLayout = ENABLE_GUI ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	//����Depth Attachment Description, Reference
	attachmentDescriptions[1].format = ChooseDepthFormat(false);
	attachmentDescriptions[1].samples = VK_SAMPLE_COUNT_1_BIT; //��ʹ�ö��ز���
	attachmentDescriptions[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; //RenderPass��ʼǰ���
	attachmentDescriptions[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; //RenderPass����������Ҫ
	attachmentDescriptions[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescriptions[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachmentDescriptions[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; //������depth stencil�Ĳ���

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
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; //��������ֻ�ύһ�Σ��Ը����Ż�

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

	//�ϸ���˵��Ҫһ��transferQueue������һ��graphicQueue��presentQueue������transfer���ܣ�Pick PhysicalDevice��ȷ��һ�£�
	vkQueueSubmit(m_GraphicQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(m_GraphicQueue); //����Ҳ����ʹ��Fence + vkWaitForFence������ͬ�����submit����

	vkFreeCommandBuffers(m_LogicalDevice, m_TransferCommandPool, 1, &commandBuffer);
}

std::vector<char> VulkanRenderer::ReadShaderFile(const std::filesystem::path& filepath)
{
	ASSERT(std::filesystem::exists(filepath), std::format("Shader file path {} not exist", filepath.string()));
	//��������ļ�ĩβ������ͳ�Ƴ���
	std::ifstream file(filepath, std::ios::ate | std::ios::binary);
	ASSERT(file.is_open(), std::format("Open shader file {} failed", filepath.string()));

	//tellg��ȡ��ǰ�ļ���дλ��
	size_t fileSize = static_cast<size_t>(file.tellg());
	std::vector<char> vecBuffer(fileSize);

	//ָ��ص��ļ���ͷ
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
	//MemoryRequirements�Ĳ������£�
	//memoryRequirements.size			�����ڴ�Ĵ�С
	//memoryRequirements.alignment		�����ڴ�Ķ��뷽ʽ����Buffer��usage��flags��������
	//memoryRequirements.memoryTypeBits �ʺϸ�Buffer���ڴ����ͣ�λֵ��

	VkMemoryRequirements memoryRequirements;
	vkGetBufferMemoryRequirements(m_LogicalDevice, buffer, &memoryRequirements);

	VkMemoryAllocateInfo memoryAllocInfo{};
	memoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocInfo.allocationSize = memoryRequirements.size;
	//�Կ��в�ͬ���͵��ڴ棬��ͬ���͵��ڴ�������Ĳ�����Ч�ʸ�����ͬ����Ҫ��������Ѱ�����ʺϵ��ڴ�����
	memoryAllocInfo.memoryTypeIndex = FindSuitableMemoryTypeIndex(memoryRequirements.memoryTypeBits, propertyFlags);

	VULKAN_ASSERT(vkAllocateMemory(m_LogicalDevice, &memoryAllocInfo, nullptr, &bufferMemory), "Allocate buffer memory failed");
}

void VulkanRenderer::CreateBufferAndBindMemory(VkDeviceSize deviceSize, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags propertyFlags, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
	VkBufferCreateInfo BufferCreateInfo{};
	BufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	BufferCreateInfo.size = deviceSize;	//Ҫ������Buffer�Ĵ�С
	BufferCreateInfo.usage = usageFlags;	//ʹ��Ŀ�ģ���������VertexBuffer��IndexBuffer������
	BufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; //Buffer���Ա����QueueFamily��������ѡ���ռģʽ
	BufferCreateInfo.flags = 0;	//�������û�����ڴ�ϡ��̶ȣ�0ΪĬ��ֵ

	VULKAN_ASSERT(vkCreateBuffer(m_LogicalDevice, &BufferCreateInfo, nullptr, &buffer), "Create buffer failed");

	AllocateBufferMemory(propertyFlags, buffer, bufferMemory);

	vkBindBufferMemory(m_LogicalDevice, buffer, bufferMemory, 0);
}


// Wrapper functions for aligned memory allocation
// There is currently no standard for this in C++ that works across all platforms and vendors, so we abstract this
static void* alignedAlloc(size_t size, size_t alignment)
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

static void alignedFree(void* data)
{
#if	defined(_MSC_VER) || defined(__MINGW32__)
	_aligned_free(data);
#else
	free(data);
#endif
}

void VulkanRenderer::CreateUniformBuffers()
{
	m_vecUniformBuffers.resize(m_vecSwapChainImages.size());
	m_vecUniformBufferMemories.resize(m_vecSwapChainImages.size());

	m_UboBufferSize = sizeof(UniformBufferObject);

	for (size_t i = 0; i < m_vecSwapChainImages.size(); ++i)
	{
		CreateBufferAndBindMemory(m_UboBufferSize,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			m_vecUniformBuffers[i],
			m_vecUniformBufferMemories[i]
		);
	}


	auto physicalDeviceProperties = m_mapPhysicalDeviceInfo.at(m_PhysicalDevice).properties;
	size_t minUboAlignment = physicalDeviceProperties.limits.minUniformBufferOffsetAlignment;
	m_DynamicAlignment = sizeof(glm::mat4) + sizeof(float);
	if (minUboAlignment > 0)
	{
		m_DynamicAlignment = (m_DynamicAlignment + minUboAlignment - 1) & ~(minUboAlignment - 1);
	}

	m_DynamicUboBufferSize = m_DynamicAlignment * INSTANCE_NUM;

	m_DynamicUboData.model = (glm::mat4*)alignedAlloc(m_DynamicUboBufferSize, minUboAlignment > 0 ? minUboAlignment : m_DynamicAlignment);
	m_DynamicUboData.fTextureIndex = (float*)((size_t)m_DynamicUboData.model + sizeof(glm::mat4));

	m_vecDynamicUniformBuffers.resize(m_vecSwapChainImages.size());
	m_vecDynamicUniformBufferMemories.resize(m_vecSwapChainImages.size());

	for (size_t i = 0; i < m_vecSwapChainImages.size(); ++i)
	{
		CreateBufferAndBindMemory(m_DynamicUboBufferSize,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			m_vecDynamicUniformBuffers[i],
			m_vecDynamicUniformBufferMemories[i]
		);
	}
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

void VulkanRenderer::CreateImageAndBindMemory(UINT uiWidth, UINT uiHeight, UINT uiMipLevelCount, UINT uiLayerCount, UINT uiFaceCount, VkSampleCountFlagBits sampleCount, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags propertyFlags, VkImage& image, VkDeviceMemory& imageMemory)
{
	VkImageCreateInfo imageCreateInfo{};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.extent.width = static_cast<uint32_t>(uiWidth);
	imageCreateInfo.extent.height = static_cast<uint32_t>(uiHeight);
	imageCreateInfo.extent.depth = 1;
	imageCreateInfo.mipLevels = uiMipLevelCount;
	imageCreateInfo.arrayLayers = (uiFaceCount == 6) ? uiFaceCount : uiLayerCount; // Cubemap��face����vulkan������arrayLayers
	imageCreateInfo.format = format;
	imageCreateInfo.tiling = tiling;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCreateInfo.usage = usage;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.samples = sampleCount;
	imageCreateInfo.flags = (uiFaceCount == 6) ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0; //Cubemap��Ҫ���ö�Ӧflags

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

void VulkanRenderer::ChangeImageLayout(VkImage image, VkFormat format, UINT uiMipLevelCount, UINT uiLayerCount, UINT uiFaceCount, VkImageLayout oldLayout, VkImageLayout newLayout)
{
	VkCommandBuffer singleTimeCommandBuffer = BeginSingleTimeCommand();

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	//����ͼ�񲼾ֵ�ת��
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	//���ڴ���Queue Family������Ȩ����ʹ��ʱ��ΪIGNORED
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	//ָ��Ŀ��ͼ�����÷�Χ
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = uiMipLevelCount;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = (uiFaceCount == 6) ? uiFaceCount : uiLayerCount;
	//ָ��barrier֮ǰ���뷢������Դ�������ͣ���barrier֮�����ȴ�����Դ��������
	//��Ҫ����oldLayout��newLayout������������
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
		srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; //�����д���������Ҫ�ȴ��κζ������ָ��һ��������ֵĹ��߽׶�
		dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT; //�����д����������ڴ���׶ν��У�α�׶Σ���ʾ���䷢����
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
		srcStage,		//������barrier֮ǰ�Ĺ��߽׶�
		dstStage,		//������barrier֮��Ĺ��߽׶�
		0,				//������ΪVK_DEPENDENCY_BY_REGION_BIT�����������򲿷ֶ�ȡ��Դ
		0, nullptr,		//Memory Barrier������
		0, nullptr,		//Buffer Memory Barrier������
		1, &barrier);	//Image Memory Barrier������

	EndSingleTimeCommand(singleTimeCommandBuffer);
}

void VulkanRenderer::TransferImageDataByStageBuffer(void* pData, VkDeviceSize imageSize, VkImage& image, UINT uiWidth, UINT uiHeight, DZW_VulkanWrap::Texture& texture, ktxTexture* pKtxTexture)
{
	if (texture.IsKtxTexture())
	{
		ASSERT(pKtxTexture);
	}
	else
	{
		ASSERT(texture.m_uiFaceNum == 1 && texture.m_uiLayerNum == 1 && texture.m_uiMipLevelNum == 1);
	}

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	CreateBufferAndBindMemory(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer, stagingBufferMemory);

	void* imageData;
	vkMapMemory(m_LogicalDevice, stagingBufferMemory, 0, imageSize, 0, (void**)&imageData);
	memcpy(imageData, pData, static_cast<size_t>(imageSize));
	vkUnmapMemory(m_LogicalDevice, stagingBufferMemory);

	VkCommandBuffer singleTimeCommandBuffer = BeginSingleTimeCommand();

	if (texture.IsKtxTexture())
	{
		std::vector<VkBufferImageCopy> vecBufferCopyRegions;
		for (UINT face = 0; face < texture.m_uiFaceNum; ++face)
		{
			for (UINT layer = 0; layer < texture.m_uiLayerNum; ++layer)
			{
				for (UINT mipLevel = 0; mipLevel < texture.m_uiMipLevelNum; ++mipLevel)
				{
					size_t offset;
					KTX_error_code ret = ktxTexture_GetImageOffset(pKtxTexture, mipLevel, layer, face, &offset);
					ASSERT(ret == KTX_SUCCESS);
					VkBufferImageCopy bufferCopyRegion = {};
					bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					bufferCopyRegion.imageSubresource.mipLevel = mipLevel;
					bufferCopyRegion.imageSubresource.baseArrayLayer = (texture.m_uiFaceNum > 1) ? (face + layer * 6) : (layer);
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
	}
	else
	{
		VkBufferImageCopy region{};
		//ָ��Ҫ���Ƶ�������buffer�е�ƫ����
		region.bufferOffset = 0;
		//ָ��������memory�еĴ�ŷ�ʽ�����ڶ���
		//����Ϊ0����������memory�л���մ��
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		//ָ�����ݱ����Ƶ�image����һ����
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = { uiWidth, uiHeight, 1 };
		vkCmdCopyBufferToImage(singleTimeCommandBuffer, stagingBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
	}

	EndSingleTimeCommand(singleTimeCommandBuffer);

	vkDestroyBuffer(m_LogicalDevice, stagingBuffer, nullptr);
	vkFreeMemory(m_LogicalDevice, stagingBufferMemory, nullptr);
}

//void VulkanRenderer::CreateTextureImageAndFillData()
//{
//	//int nTexWidth = 0;
//	//int nTexHeight = 0;
//	//int nTexChannel = 0;
//	//stbi_uc* pixels = stbi_load(m_TexturePath.string().c_str(), &nTexWidth, &nTexHeight, &nTexChannel, STBI_rgb_alpha);
//	//ASSERT(pixels, std::format("Stb load image {} failed", m_TexturePath.string()));
//
//	//VkDeviceSize imageSize = (uint64_t)nTexWidth * (uint64_t)nTexHeight * 4;
//
//	ktxResult result;
//	ktxTexture* ktxTexture;
//	result = ktxTexture_CreateFromNamedFile(m_TexturePath.string().c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTexture);
//	ASSERT(result == KTX_SUCCESS, "KTX load image data failed");
//
//	ktx_uint8_t* ktxTextureData = ktxTexture_GetData(ktxTexture);
//	ktx_size_t ktxTextureSize = ktxTexture_GetSize(ktxTexture);
//
//	UINT uiTexWidth = ktxTexture->baseWidth;
//	UINT uiTexHeight = ktxTexture->baseHeight;
//
//	CreateImageAndBindMemory(uiTexWidth, uiTexHeight, m_uiMipmapLevel,
//		VK_SAMPLE_COUNT_1_BIT,
//		VK_FORMAT_R8G8B8A8_SRGB,
//		VK_IMAGE_TILING_OPTIMAL,
//		VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
//		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
//		m_TextureImage, m_TextureImageMemory);
//
//	//copy֮ǰ����layout�ӳ�ʼ��undefinedתΪtransfer dst
//	ChangeImageLayout(m_TextureImage,
//		VK_FORMAT_R8G8B8A8_SRGB,				//image format
//		m_uiMipmapLevel,						//mipmap level
//		VK_IMAGE_LAYOUT_UNDEFINED,				//src layout
//		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);	//dst layout
//
//	TransferImageDataByStageBuffer(ktxTextureData, ktxTextureSize, m_TextureImage, static_cast<UINT>(uiTexWidth), static_cast<UINT>(uiTexHeight));
//
//	//transfer֮�󣬽�layout��transfer dstתΪshader readonly
//	//���ʹ��mipmap��֮����generateMipmaps�н�layoutתΪshader readonly
//
//	ChangeImageLayout(m_TextureImage,
//		VK_FORMAT_R8G8B8A8_SRGB, 
//		m_uiMipmapLevel,
//		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
//		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
//
//	//generateMipmaps(m_TextureImage, VK_FORMAT_R8G8B8A8_SRGB, nTexWidth, nTexHeight, m_uiMipmapLevel);
//
//	//stbi_image_free(pixels);
//}
//
//void VulkanRenderer::CreateTextureImageView()
//{
//	m_TextureImageView = CreateImageView(m_TextureImage,
//		VK_FORMAT_R8G8B8A8_SRGB,	//��ʽΪsRGB
//		VK_IMAGE_ASPECT_COLOR_BIT,	//aspectFlagsΪCOLOR_BIT
//		m_uiMipmapLevel
//	);
//}

void VulkanRenderer::CreateDescriptorSetLayout()
{
	//UniformBufferObject Binding
	VkDescriptorSetLayoutBinding uboLayoutBinding{};
	uboLayoutBinding.binding = 0; //��ӦVertex Shader�е�layout binding
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; //ֻ��Ҫ��vertex stage��Ч
	uboLayoutBinding.pImmutableSamplers = nullptr;

	//Dynamic UniformBufferObject Binding
	VkDescriptorSetLayoutBinding dynamicUboLayoutBinding{};
	dynamicUboLayoutBinding.binding = 1; //��ӦVertex Shader�е�layout binding
	dynamicUboLayoutBinding.descriptorCount = 1;
	dynamicUboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	dynamicUboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; //ֻ��Ҫ��vertex stage��Ч
	dynamicUboLayoutBinding.pImmutableSamplers = nullptr;

	//CombinedImageSampler Binding
	VkDescriptorSetLayoutBinding samplerLayoutBinding{};
	samplerLayoutBinding.binding = 2; ////��ӦFragment Shader�е�layout binding
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; //ֻ����fragment stage
	samplerLayoutBinding.pImmutableSamplers = nullptr;

	std::vector<VkDescriptorSetLayoutBinding> vecDescriptorLayoutBinding = {
		uboLayoutBinding,
		dynamicUboLayoutBinding,
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
	uboPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboPoolSize.descriptorCount = static_cast<UINT>(m_vecSwapChainImages.size());

	//dynamic ubo
	VkDescriptorPoolSize dynamicUboPoolSize{};
	dynamicUboPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	dynamicUboPoolSize.descriptorCount = static_cast<UINT>(m_vecSwapChainImages.size());

	//sampler
	VkDescriptorPoolSize samplerPoolSize{};
	samplerPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerPoolSize.descriptorCount = static_cast<UINT>(m_vecSwapChainImages.size());

	std::vector<VkDescriptorPoolSize> vecPoolSize = {
		uboPoolSize,
		dynamicUboPoolSize,
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
		VkDescriptorBufferInfo descriptorBufferInfo{};
		descriptorBufferInfo.buffer = m_vecUniformBuffers[i];
		descriptorBufferInfo.offset = 0;
		descriptorBufferInfo.range = sizeof(UniformBufferObject);

		VkWriteDescriptorSet uboWrite{};
		uboWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		uboWrite.dstSet = m_vecDescriptorSets[i];
		uboWrite.dstBinding = 0;
		uboWrite.dstArrayElement = 0;
		uboWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboWrite.descriptorCount = 1;
		uboWrite.pBufferInfo = &descriptorBufferInfo;

		//dynamic ubo
		VkDescriptorBufferInfo dynamicDescriptorBufferInfo{};
		dynamicDescriptorBufferInfo.buffer = m_vecDynamicUniformBuffers[i];
		dynamicDescriptorBufferInfo.offset = 0;
		dynamicDescriptorBufferInfo.range = m_DynamicAlignment;

		VkWriteDescriptorSet dynamicUboWrite{};
		dynamicUboWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		dynamicUboWrite.dstSet = m_vecDescriptorSets[i];
		dynamicUboWrite.dstBinding = 1;
		dynamicUboWrite.dstArrayElement = 0;
		dynamicUboWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		dynamicUboWrite.descriptorCount = 1;
		dynamicUboWrite.pBufferInfo = &dynamicDescriptorBufferInfo;

		//sampler
		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = m_Texture.m_ImageView;
		imageInfo.sampler = m_Texture.m_Sampler;

		VkWriteDescriptorSet samplerWrite{};
		samplerWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		samplerWrite.dstSet = m_vecDescriptorSets[i];
		samplerWrite.dstBinding = 2;
		samplerWrite.dstArrayElement = 0;
		samplerWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerWrite.descriptorCount = 1;
		samplerWrite.pImageInfo = &imageInfo;

		std::vector<VkWriteDescriptorSet> vecDescriptorWrite = {
			uboWrite,
			dynamicUboWrite,
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

void VulkanRenderer::CreateGraphicPipelineLayout()
{
	//-----------------------Pipeline Layout--------------------------//
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &m_DescriptorSetLayout;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

	VULKAN_ASSERT(vkCreatePipelineLayout(m_LogicalDevice, &pipelineLayoutCreateInfo, nullptr, &m_GraphicPipelineLayout), "Create pipeline layout failed");
}

void VulkanRenderer::CreateGraphicPipeline()
{
	/****************************�ɱ�̹���*******************************/

	ASSERT(m_mapShaderModule.find(VK_SHADER_STAGE_VERTEX_BIT) != m_mapShaderModule.end(), "No vertex shader module");
	ASSERT(m_mapShaderModule.find(VK_SHADER_STAGE_FRAGMENT_BIT) != m_mapShaderModule.end(), "No fragment shader module");

	VkPipelineShaderStageCreateInfo vertShaderStageCreateInfo{};
	vertShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageCreateInfo.module = m_mapShaderModule.at(VK_SHADER_STAGE_VERTEX_BIT); //Bytecode
	vertShaderStageCreateInfo.pName = "main"; //Ҫinvoke�ĺ���

	VkPipelineShaderStageCreateInfo fragShaderStageCreateInfo{};
	fragShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageCreateInfo.module = m_mapShaderModule.at(VK_SHADER_STAGE_FRAGMENT_BIT); //Bytecode
	fragShaderStageCreateInfo.pName = "main"; //Ҫinvoke�ĺ���

	VkPipelineShaderStageCreateInfo shaderStageCreateInfos[] = {
		vertShaderStageCreateInfo,
		fragShaderStageCreateInfo,
	};

	/*****************************�̶�����*******************************/

	//-----------------------Dynamic State--------------------------//
	//һ��ὫViewport��Scissor��Ϊdynamic���Է�����ʱ�޸�
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
	//���û�а�Viewport��Scissor��Ϊdynamic state������Ҫ�ڴ˴�ָ����ʹ���������÷��������õ�Viewport�ǲ��ɱ�ģ�
	if (!m_bViewportAndScissorIsDynamic)
	{
		//����Viewport����ΧΪ[0,0]��[width,height]
		VkViewport viewport{};
		viewport.x = 0.f;
		viewport.y = 0.f;
		viewport.width = static_cast<float>(m_SwapChainExtent2D.width);
		viewport.height = static_cast<float>(m_SwapChainExtent2D.height);
		viewport.minDepth = 0.f;
		viewport.maxDepth = 1.f;

		//���òü�����Scissor Rectangle
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
	rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;	//�����󣬳���Զ��ƽ��Ĳ��ֻᱻ�ض���Զ��ƽ���ϣ������Ƕ���
	rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;	//�����󣬽�ֹ����ͼԪ������դ����
	rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;	//ͼԪģʽ��������FILL��LINE��POINT
	rasterizationStateCreateInfo.lineWidth = 1.f;	//ָ����դ������߶ο��
	rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;	//�޳�ģʽ��������NONE��FRONT��BACK��FRONT_AND_BACK
	rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; //�����򣬿�����˳ʱ��cw����ʱ��ccw
	rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE; //���ƫ�ƣ�һ������Shaodw Map�б�����Ӱ�
	rasterizationStateCreateInfo.depthBiasConstantFactor = 0.f;
	rasterizationStateCreateInfo.depthBiasClamp = 0.f;
	rasterizationStateCreateInfo.depthBiasSlopeFactor = 0.f;

	//-----------------------Multisample State--------------------------//
	VkPipelineMultisampleStateCreateInfo multisamplingStateCreateInfo{};
	multisamplingStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisamplingStateCreateInfo.sampleShadingEnable = (VkBool32)(m_Texture.m_uiMipLevelNum > 1);
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
	depthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
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

	/***********************************************************************/
	VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.stageCount = static_cast<UINT>(m_mapShaderModule.size());
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
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; //��ֵΪsignaled

	for (int i = 0; i < m_vecSwapChainImages.size(); ++i)
	{
		VULKAN_ASSERT(vkCreateSemaphore(m_LogicalDevice, &semaphoreCreateInfo, nullptr, &m_vecImageAvailableSemaphores[i]), "Create image available semaphore failed");
		VULKAN_ASSERT(vkCreateSemaphore(m_LogicalDevice, &semaphoreCreateInfo, nullptr, &m_vecRenderFinishedSemaphores[i]), "Create render finished semaphore failed");
		VULKAN_ASSERT(vkCreateFence(m_LogicalDevice, &fenceCreateInfo, nullptr, &m_vecInFlightFences[i]), "Create inflight fence failed");
	}
}

void VulkanRenderer::SetupCamera()
{
	m_Camera.Set(45.f, (float)m_SwapChainExtent2D.width / (float)m_SwapChainExtent2D.height, 0.1f, 10000.f);
	m_Camera.SetViewportSize(static_cast<float>(m_SwapChainExtent2D.width), static_cast<float>(m_SwapChainExtent2D.height));
	m_Camera.SetWindow(m_pWindow);
	m_Camera.SetPosition({ 0.f, 0.f, 100.f });

	glfwSetWindowUserPointer(m_pWindow, (void*)&m_Camera);

	glfwSetScrollCallback(m_pWindow, [](GLFWwindow* window, double dOffsetX, double dOffsetY)
		{
			if (ENABLE_GUI && ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
				return;
			auto& camera = *(Camera*)glfwGetWindowUserPointer(window);
			camera.OnMouseScroll(dOffsetX, dOffsetY);
		}
	);
}

void VulkanRenderer::CreateSkyboxShader()
{
	std::unordered_map<VkShaderStageFlagBits, std::filesystem::path> mapSkyboxShaderPath = {
		{ VK_SHADER_STAGE_VERTEX_BIT,	"./Assert/Shader/Skybox/vert.spv" },
		{ VK_SHADER_STAGE_FRAGMENT_BIT,	"./Assert/Shader/Skybox/frag.spv" },
	};
	ASSERT(mapSkyboxShaderPath.size() > 0, "Detect no shader spv file");

	m_mapSkyboxShaderModule.clear();

	for (const auto& spvPath : mapSkyboxShaderPath)
	{
		auto shaderModule = CreateShaderModule(ReadShaderFile(spvPath.second));

		m_mapSkyboxShaderModule[spvPath.first] = shaderModule;
	}
}

void VulkanRenderer::CreateSkyboxGraphicPipelineLayout()
{
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &m_SkyboxDescriptorSetLayout;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

	VULKAN_ASSERT(vkCreatePipelineLayout(m_LogicalDevice, &pipelineLayoutCreateInfo, nullptr, &m_SkyboxGraphicPipelineLayout), "Create skybox pipeline layout failed");
}

void VulkanRenderer::CreateSkyboxGraphicPipeline()
{
	/****************************�ɱ�̹���*******************************/
	VkPipelineShaderStageCreateInfo vertShaderStageCreateInfo{};
	vertShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageCreateInfo.module = m_mapSkyboxShaderModule.at(VK_SHADER_STAGE_VERTEX_BIT); //Bytecode
	vertShaderStageCreateInfo.pName = "main"; //Ҫinvoke�ĺ���

	VkPipelineShaderStageCreateInfo fragShaderStageCreateInfo{};
	fragShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageCreateInfo.module = m_mapSkyboxShaderModule.at(VK_SHADER_STAGE_FRAGMENT_BIT); //Bytecode
	fragShaderStageCreateInfo.pName = "main"; //Ҫinvoke�ĺ���

	VkPipelineShaderStageCreateInfo shaderStageCreateInfos[] = {
		vertShaderStageCreateInfo,
		fragShaderStageCreateInfo,
	};

	/*****************************�̶�����*******************************/

	//-----------------------Dynamic State--------------------------//

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

	//����Viewport����ΧΪ[0,0]��[width,height]
	VkViewport viewport{};
	viewport.x = 0.f;
	viewport.y = 0.f;
	viewport.width = static_cast<float>(m_SwapChainExtent2D.width);
	viewport.height = static_cast<float>(m_SwapChainExtent2D.height);
	viewport.minDepth = 0.f;
	viewport.maxDepth = 1.f;

	//���òü�����Scissor Rectangle
	VkRect2D scissor{};
	scissor.offset = { 0,0 };
	VkExtent2D ScissorExtent;
	ScissorExtent.width = m_SwapChainExtent2D.width;
	ScissorExtent.height = m_SwapChainExtent2D.height;
	scissor.extent = ScissorExtent;

	viewportStateCreateInfo.pViewports = &viewport;
	viewportStateCreateInfo.pScissors = &scissor;

	//-----------------------Raserization State--------------------------//
	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo{};
	rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;	//�����󣬳���Զ��ƽ��Ĳ��ֻᱻ�ض���Զ��ƽ���ϣ������Ƕ���
	rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;	//�����󣬽�ֹ����ͼԪ������դ����
	rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;	//ͼԪģʽ��������FILL��LINE��POINT
	rasterizationStateCreateInfo.lineWidth = 1.f;	//ָ����դ������߶ο��
	rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_FRONT_BIT;	//�޳�����Ŀɼ���
	rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; //�����򣬿�����˳ʱ��cw����ʱ��ccw
	rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE; //���ƫ�ƣ�һ������Shaodw Map�б�����Ӱ�
	rasterizationStateCreateInfo.depthBiasConstantFactor = 0.f;
	rasterizationStateCreateInfo.depthBiasClamp = 0.f;
	rasterizationStateCreateInfo.depthBiasSlopeFactor = 0.f;

	//-----------------------Multisample State--------------------------//
	VkPipelineMultisampleStateCreateInfo multisamplingStateCreateInfo{};
	multisamplingStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisamplingStateCreateInfo.sampleShadingEnable = VK_FALSE;
	multisamplingStateCreateInfo.minSampleShading = 0.8f;
	multisamplingStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisamplingStateCreateInfo.minSampleShading = 1.f;
	multisamplingStateCreateInfo.pSampleMask = nullptr;
	multisamplingStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
	multisamplingStateCreateInfo.alphaToOneEnable = VK_FALSE;

	//-----------------------Depth Stencil State--------------------------//
	VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo{};
	depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilStateCreateInfo.depthTestEnable = VK_FALSE; //��Ϊ������ʼ������Զ������������ȼ��
	depthStencilStateCreateInfo.depthWriteEnable = VK_FALSE;
	depthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
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

	/***********************************************************************/
	VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.stageCount = static_cast<UINT>(m_mapSkyboxShaderModule.size());
	pipelineCreateInfo.pStages = shaderStageCreateInfos;
	pipelineCreateInfo.pDynamicState = VK_NULL_HANDLE;
	pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
	pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	pipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
	pipelineCreateInfo.pMultisampleState = &multisamplingStateCreateInfo;
	pipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
	pipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
	pipelineCreateInfo.layout = m_SkyboxGraphicPipelineLayout;
	pipelineCreateInfo.renderPass = m_RenderPass;
	pipelineCreateInfo.subpass = 0;
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineCreateInfo.basePipelineIndex = -1;

	VULKAN_ASSERT(vkCreateGraphicsPipelines(m_LogicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &m_SkyboxGraphicPipeline), "Create skybox graphic pipeline failed");
}

void VulkanRenderer::CalcMeshGridVertexData()
{
	ASSERT(m_fMeshGridSplit >= 1);
	m_vecMeshGridVertices.clear();
	UINT uiMeshGridSplitPointCount = static_cast<UINT>(m_fMeshGridSplit) + 1;
	m_vecMeshGridVertices.resize(uiMeshGridSplitPointCount * uiMeshGridSplitPointCount);
	float fSplitLen = 1.f / m_fMeshGridSplit;
	for (UINT i = 0; i < uiMeshGridSplitPointCount; ++i)
	{
		for (UINT j = 0; j < uiMeshGridSplitPointCount; ++j)
		{
			UINT uiIdx = i * uiMeshGridSplitPointCount + j;
			float x = static_cast<float>(j) * fSplitLen - 0.5f;
			float y = static_cast<float>(i) * fSplitLen - 0.5f;
			m_vecMeshGridVertices[uiIdx].pos = {x, y, 0.f};
		}
	}
}

void VulkanRenderer::CalcMeshGridIndexData()
{
	UINT uiVertexCount = m_vecMeshGridVertices.size();
	ASSERT(uiVertexCount >= 4);
	m_vecMeshGridIndices.clear();
	UINT uiMeshGridSplitLineCount = static_cast<UINT>(m_fMeshGridSplit);
	UINT uiMeshGridSplitPointCount = static_cast<UINT>(m_fMeshGridSplit) + 1;
	
	//����
	for (UINT i = 0; i < uiMeshGridSplitPointCount; ++i)
	{
		for (UINT j = 0; j < uiMeshGridSplitLineCount; ++j)
		{
			UINT uiStart = i * uiMeshGridSplitPointCount + j;
			m_vecMeshGridIndices.push_back(uiStart);
			m_vecMeshGridIndices.push_back(uiStart + 1);
		}
	}

	//����
	for (UINT i = 0; i < uiMeshGridSplitPointCount; ++i)
	{
		for (UINT j = 0; j < uiMeshGridSplitLineCount; ++j)
		{
			UINT uiStart = i + j * uiMeshGridSplitPointCount;
			m_vecMeshGridIndices.push_back(uiStart);
			m_vecMeshGridIndices.push_back(uiStart + uiMeshGridSplitPointCount);
		}
	}
}

void VulkanRenderer::RecreateMeshGrid()
{
	ENABLE_GUI = false;
	vkQueueWaitIdle(m_GraphicQueue);
	vkDeviceWaitIdle(m_LogicalDevice);

	vkDestroyBuffer(m_LogicalDevice, m_MeshGridVertexBuffer, nullptr);
	vkFreeMemory(m_LogicalDevice, m_MeshGridVertexBufferMemory, nullptr);

	vkDestroyBuffer(m_LogicalDevice, m_MeshGridIndexBuffer, nullptr);
	vkFreeMemory(m_LogicalDevice, m_MeshGridIndexBufferMemory, nullptr);

	CreateMeshGridVertexBuffer();
	CreateMeshGridIndexBuffer();
	ENABLE_GUI = true;
}

void VulkanRenderer::CreateMeshGridVertexBuffer()
{
	CalcMeshGridVertexData();

	for (auto& vert : m_vecMeshGridVertices)
		vert.pos *= (m_fMeshGridSize / 2.f);

	Log::Info(std::format("Mesh Grid Vertex Count : {}", m_vecMeshGridVertices.size()));
	ASSERT(m_vecMeshGridVertices.size() > 0, "Vertex data empty");
	VkDeviceSize verticesSize = sizeof(m_vecMeshGridVertices[0]) * m_vecMeshGridVertices.size();
	CreateBufferAndBindMemory(verticesSize,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		m_MeshGridVertexBuffer, m_MeshGridVertexBufferMemory);
	TransferBufferDataByStageBuffer(m_vecMeshGridVertices.data(), verticesSize, m_MeshGridVertexBuffer);
}

void VulkanRenderer::CreateMeshGridIndexBuffer()
{
	CalcMeshGridIndexData();

	Log::Info(std::format("Mesh Grid Index Count : {}", m_vecMeshGridIndices.size()));
	VkDeviceSize indicesSize = sizeof(m_vecMeshGridIndices[0]) * m_vecMeshGridIndices.size();
	if (indicesSize > 0)
		CreateBufferAndBindMemory(indicesSize,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			m_MeshGridIndexBuffer, m_MeshGridIndexBufferMemory);
	TransferBufferDataByStageBuffer(m_vecMeshGridIndices.data(), indicesSize, m_MeshGridIndexBuffer);
}

void VulkanRenderer::CreateMeshGridShader()
{
	std::unordered_map<VkShaderStageFlagBits, std::filesystem::path> mapMeshGridShaderPath = {
		{ VK_SHADER_STAGE_VERTEX_BIT,	"./Assert/Shader/MeshGrid/vert.spv" },
		{ VK_SHADER_STAGE_FRAGMENT_BIT,	"./Assert/Shader/MeshGrid/frag.spv" },
	};
	ASSERT(mapMeshGridShaderPath.size() > 0, "Detect no shader spv file");

	m_mapMeshGridShaderModule.clear();

	for (const auto& spvPath : mapMeshGridShaderPath)
	{
		auto shaderModule = CreateShaderModule(ReadShaderFile(spvPath.second));

		m_mapMeshGridShaderModule[spvPath.first] = shaderModule;
	}
}

void VulkanRenderer::CreateMeshGridUniformBuffers()
{
	m_vecMeshGridUniformBuffers.resize(m_vecSwapChainImages.size());
	m_vecMeshGridUniformBufferMemories.resize(m_vecSwapChainImages.size());

	for (size_t i = 0; i < m_vecSwapChainImages.size(); ++i)
	{
		CreateBufferAndBindMemory(sizeof(MeshGridUniformBufferObject),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			m_vecMeshGridUniformBuffers[i],
			m_vecMeshGridUniformBufferMemories[i]
		);
	}
}

void VulkanRenderer::UpdateMeshGridUniformBuffer(UINT uiIdx)
{
	m_MeshGridUboData.model = glm::translate(glm::mat4(1.f), { 0.f, 0.f, 0.f }) * glm::rotate(glm::mat4(1.f), glm::radians(0.f), glm::vec3(1.0, 0.0, 0.0));
	m_MeshGridUboData.view = m_Camera.GetViewMatrix();
	m_MeshGridUboData.proj = m_Camera.GetProjMatrix();
	//OpenGL��Vulkan�Ĳ��� - Y�����Ƿ���
	m_MeshGridUboData.proj[1][1] *= -1.f;

	void* uniformBufferData;
	vkMapMemory(m_LogicalDevice, m_vecMeshGridUniformBufferMemories[uiIdx], 0, sizeof(MeshGridUniformBufferObject), 0, &uniformBufferData);
	memcpy(uniformBufferData, &m_MeshGridUboData, sizeof(MeshGridUniformBufferObject));
	vkUnmapMemory(m_LogicalDevice, m_vecMeshGridUniformBufferMemories[uiIdx]);
}

void VulkanRenderer::CreateMeshGridDescriptorSetLayout()
{
	//UniformBufferObject Binding
	VkDescriptorSetLayoutBinding uboLayoutBinding{};
	uboLayoutBinding.binding = 0; //��ӦVertex Shader�е�layout binding
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; //ֻ��Ҫ��vertex stage��Ч
	uboLayoutBinding.pImmutableSamplers = nullptr;

	std::vector<VkDescriptorSetLayoutBinding> vecDescriptorLayoutBinding = {
		uboLayoutBinding,
	};

	VkDescriptorSetLayoutCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	createInfo.bindingCount = static_cast<UINT>(vecDescriptorLayoutBinding.size());
	createInfo.pBindings = vecDescriptorLayoutBinding.data();

	VULKAN_ASSERT(vkCreateDescriptorSetLayout(m_LogicalDevice, &createInfo, nullptr, &m_MeshGridDescriptorSetLayout), "Create mesh grid descriptor layout failed");
}

void VulkanRenderer::CreateMeshGridDescriptorPool()
{
	//ubo
	VkDescriptorPoolSize uboPoolSize{};
	uboPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboPoolSize.descriptorCount = static_cast<UINT>(m_vecSwapChainImages.size());

	std::vector<VkDescriptorPoolSize> vecPoolSize = {
		uboPoolSize,
	};

	VkDescriptorPoolCreateInfo poolCreateInfo{};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCreateInfo.poolSizeCount = static_cast<UINT>(vecPoolSize.size());
	poolCreateInfo.pPoolSizes = vecPoolSize.data();
	poolCreateInfo.maxSets = static_cast<UINT>(m_vecSwapChainImages.size());

	VULKAN_ASSERT(vkCreateDescriptorPool(m_LogicalDevice, &poolCreateInfo, nullptr, &m_MeshGridDescriptorPool), "Create mesh grid descriptor pool failed");
}

void VulkanRenderer::CreateMeshGridDescriptorSets()
{
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorSetCount = static_cast<UINT>(m_vecSwapChainImages.size());
	allocInfo.descriptorPool = m_MeshGridDescriptorPool;

	std::vector<VkDescriptorSetLayout> vecDupDescriptorSetLayout(m_vecSwapChainImages.size(), m_MeshGridDescriptorSetLayout);
	allocInfo.pSetLayouts = vecDupDescriptorSetLayout.data();

	m_vecMeshGridDescriptorSets.resize(m_vecSwapChainImages.size());
	VULKAN_ASSERT(vkAllocateDescriptorSets(m_LogicalDevice, &allocInfo, m_vecMeshGridDescriptorSets.data()), "Allocate mesh grid desctiprot sets failed");

	for (size_t i = 0; i < m_vecSwapChainImages.size(); ++i)
	{
		//ubo
		VkDescriptorBufferInfo descriptorBufferInfo{};
		descriptorBufferInfo.buffer = m_vecMeshGridUniformBuffers[i];
		descriptorBufferInfo.offset = 0;
		descriptorBufferInfo.range = sizeof(MeshGridUniformBufferObject);

		VkWriteDescriptorSet uboWrite{};
		uboWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		uboWrite.dstSet = m_vecMeshGridDescriptorSets[i];
		uboWrite.dstBinding = 0;
		uboWrite.dstArrayElement = 0;
		uboWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboWrite.descriptorCount = 1;
		uboWrite.pBufferInfo = &descriptorBufferInfo;

		std::vector<VkWriteDescriptorSet> vecDescriptorWrite = {
			uboWrite,
		};

		vkUpdateDescriptorSets(m_LogicalDevice, static_cast<UINT>(vecDescriptorWrite.size()), vecDescriptorWrite.data(), 0, nullptr);
	}
}

void VulkanRenderer::CreateMeshGridGraphicPipelineLayout()
{
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &m_MeshGridDescriptorSetLayout;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

	VULKAN_ASSERT(vkCreatePipelineLayout(m_LogicalDevice, &pipelineLayoutCreateInfo, nullptr, &m_MeshGridGraphicPipelineLayout), "Create mesh grid pipeline layout failed");
}

void VulkanRenderer::CreateMeshGridGraphicPipeline()
{
	/****************************�ɱ�̹���*******************************/
	VkPipelineShaderStageCreateInfo vertShaderStageCreateInfo{};
	vertShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageCreateInfo.module = m_mapMeshGridShaderModule.at(VK_SHADER_STAGE_VERTEX_BIT); //Bytecode
	vertShaderStageCreateInfo.pName = "main"; //Ҫinvoke�ĺ���

	VkPipelineShaderStageCreateInfo fragShaderStageCreateInfo{};
	fragShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageCreateInfo.module = m_mapMeshGridShaderModule.at(VK_SHADER_STAGE_FRAGMENT_BIT); //Bytecode
	fragShaderStageCreateInfo.pName = "main"; //Ҫinvoke�ĺ���

	VkPipelineShaderStageCreateInfo shaderStageCreateInfos[] = {
		vertShaderStageCreateInfo,
		fragShaderStageCreateInfo,
	};

	/*****************************�̶�����*******************************/

	//-----------------------Dynamic State--------------------------//
	//VK_DYNAMIC_STATE_LINE_WIDTH

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
	inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
	inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;


	//-----------------------Viewport State--------------------------//
	VkPipelineViewportStateCreateInfo viewportStateCreateInfo{};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.scissorCount = 1;

	//����Viewport����ΧΪ[0,0]��[width,height]
	VkViewport viewport{};
	viewport.x = 0.f;
	viewport.y = 0.f;
	viewport.width = static_cast<float>(m_SwapChainExtent2D.width);
	viewport.height = static_cast<float>(m_SwapChainExtent2D.height);
	viewport.minDepth = 0.f;
	viewport.maxDepth = 1.f;

	//���òü�����Scissor Rectangle
	VkRect2D scissor{};
	scissor.offset = { 0,0 };
	VkExtent2D ScissorExtent;
	ScissorExtent.width = m_SwapChainExtent2D.width;
	ScissorExtent.height = m_SwapChainExtent2D.height;
	scissor.extent = ScissorExtent;

	viewportStateCreateInfo.pViewports = &viewport;
	viewportStateCreateInfo.pScissors = &scissor;

	//-----------------------Raserization State--------------------------//
	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo{};
	rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;	//�����󣬳���Զ��ƽ��Ĳ��ֻᱻ�ض���Զ��ƽ���ϣ������Ƕ���
	rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;	//�����󣬽�ֹ����ͼԪ������դ����
	rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_LINE;	//ͼԪģʽ��������FILL��LINE��POINT
	rasterizationStateCreateInfo.lineWidth = m_fMeshGridLineWidth;	//ָ����դ������߶ο��
	rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE;
	rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; //�����򣬿�����˳ʱ��cw����ʱ��ccw
	rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE; //���ƫ�ƣ�һ������Shaodw Map�б�����Ӱ�
	rasterizationStateCreateInfo.depthBiasConstantFactor = 0.f;
	rasterizationStateCreateInfo.depthBiasClamp = 0.f;
	rasterizationStateCreateInfo.depthBiasSlopeFactor = 0.f;

	//-----------------------Multisample State--------------------------//
	VkPipelineMultisampleStateCreateInfo multisamplingStateCreateInfo{};
	multisamplingStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisamplingStateCreateInfo.sampleShadingEnable = VK_FALSE;
	multisamplingStateCreateInfo.minSampleShading = 0.8f;
	multisamplingStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisamplingStateCreateInfo.minSampleShading = 1.f;
	multisamplingStateCreateInfo.pSampleMask = nullptr;
	multisamplingStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
	multisamplingStateCreateInfo.alphaToOneEnable = VK_FALSE;

	//-----------------------Depth Stencil State--------------------------//
	VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo{};
	depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilStateCreateInfo.depthTestEnable = VK_TRUE; //��Ϊ������ʼ������Զ������������ȼ��
	depthStencilStateCreateInfo.depthWriteEnable = VK_TRUE;
	depthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
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

	/***********************************************************************/
	VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.stageCount = static_cast<UINT>(m_mapMeshGridShaderModule.size());
	pipelineCreateInfo.pStages = shaderStageCreateInfos;
	pipelineCreateInfo.pDynamicState = VK_NULL_HANDLE;
	pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
	pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	pipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
	pipelineCreateInfo.pMultisampleState = &multisamplingStateCreateInfo;
	pipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
	pipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
	pipelineCreateInfo.layout = m_MeshGridGraphicPipelineLayout;
	pipelineCreateInfo.renderPass = m_RenderPass;
	pipelineCreateInfo.subpass = 0;
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineCreateInfo.basePipelineIndex = -1;

	VULKAN_ASSERT(vkCreateGraphicsPipelines(m_LogicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &m_MeshGridGraphicPipeline), "Create mesh grid graphic pipeline failed");
}

void VulkanRenderer::CreateSkyboxUniformBuffers()
{
	m_vecSkyboxUniformBuffers.resize(m_vecSwapChainImages.size());
	m_vecSkyboxUniformBufferMemories.resize(m_vecSwapChainImages.size());

	for (size_t i = 0; i < m_vecSwapChainImages.size(); ++i)
	{
		CreateBufferAndBindMemory(sizeof(SkyboxUniformBufferObject),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			m_vecSkyboxUniformBuffers[i],
			m_vecSkyboxUniformBufferMemories[i]
		);
	}
}

void VulkanRenderer::UpdateSkyboxUniformBuffer(UINT uiIdx)
{
	m_SkyboxUboData.modelView = m_Camera.GetViewMatrix();
	m_SkyboxUboData.modelView[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f); //�Ƴ�ƽ�Ʒ���
	m_SkyboxUboData.proj = m_Camera.GetProjMatrix();
	//OpenGL��Vulkan�Ĳ��� - Y�����Ƿ���
	m_SkyboxUboData.proj[1][1] *= -1.f;

	void* uniformBufferData;
	vkMapMemory(m_LogicalDevice, m_vecSkyboxUniformBufferMemories[uiIdx], 0, sizeof(SkyboxUniformBufferObject), 0, &uniformBufferData);
	memcpy(uniformBufferData, &m_SkyboxUboData, sizeof(SkyboxUniformBufferObject));
	vkUnmapMemory(m_LogicalDevice, m_vecSkyboxUniformBufferMemories[uiIdx]);
}

void VulkanRenderer::CreateSkyboxDescriptorSetLayout()
{
	//UniformBufferObject Binding
	VkDescriptorSetLayoutBinding uboLayoutBinding{};
	uboLayoutBinding.binding = 0; //��ӦVertex Shader�е�layout binding
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; //ֻ��Ҫ��vertex stage��Ч
	uboLayoutBinding.pImmutableSamplers = nullptr;

	//Cubemap ImageSampler Binding
	VkDescriptorSetLayoutBinding CubemapSamplerLayoutBinding{};
	CubemapSamplerLayoutBinding.binding = 1; ////��ӦFragment Shader�е�layout binding
	CubemapSamplerLayoutBinding.descriptorCount = 1;
	CubemapSamplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	CubemapSamplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; //ֻ����fragment stage
	CubemapSamplerLayoutBinding.pImmutableSamplers = nullptr;

	std::vector<VkDescriptorSetLayoutBinding> vecDescriptorLayoutBinding = {
		uboLayoutBinding,
		CubemapSamplerLayoutBinding,
	};

	VkDescriptorSetLayoutCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	createInfo.bindingCount = static_cast<UINT>(vecDescriptorLayoutBinding.size());
	createInfo.pBindings = vecDescriptorLayoutBinding.data();

	VULKAN_ASSERT(vkCreateDescriptorSetLayout(m_LogicalDevice, &createInfo, nullptr, &m_SkyboxDescriptorSetLayout), "Create skybox descriptor layout failed");
}

void VulkanRenderer::CreateSkyboxDescriptorPool()
{
	//ubo
	VkDescriptorPoolSize uboPoolSize{};
	uboPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboPoolSize.descriptorCount = static_cast<UINT>(m_vecSwapChainImages.size());

	//cubemap sampler
	VkDescriptorPoolSize cubemapSamplerPoolSize{};
	cubemapSamplerPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	cubemapSamplerPoolSize.descriptorCount = static_cast<UINT>(m_vecSwapChainImages.size());

	std::vector<VkDescriptorPoolSize> vecPoolSize = {
		uboPoolSize,
		cubemapSamplerPoolSize,
	};

	VkDescriptorPoolCreateInfo poolCreateInfo{};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCreateInfo.poolSizeCount = static_cast<UINT>(vecPoolSize.size());
	poolCreateInfo.pPoolSizes = vecPoolSize.data();
	poolCreateInfo.maxSets = static_cast<UINT>(m_vecSwapChainImages.size());

	VULKAN_ASSERT(vkCreateDescriptorPool(m_LogicalDevice, &poolCreateInfo, nullptr, &m_SkyboxDescriptorPool), "Create skybox descriptor pool failed");
}

void VulkanRenderer::CreateSkyboxDescriptorSets()
{
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorSetCount = static_cast<UINT>(m_vecSwapChainImages.size());
	allocInfo.descriptorPool = m_SkyboxDescriptorPool;

	std::vector<VkDescriptorSetLayout> vecDupDescriptorSetLayout(m_vecSwapChainImages.size(), m_SkyboxDescriptorSetLayout);
	allocInfo.pSetLayouts = vecDupDescriptorSetLayout.data();

	m_vecSkyboxDescriptorSets.resize(m_vecSwapChainImages.size());
	VULKAN_ASSERT(vkAllocateDescriptorSets(m_LogicalDevice, &allocInfo, m_vecSkyboxDescriptorSets.data()), "Allocate skybox desctiprot sets failed");

	for (size_t i = 0; i < m_vecSwapChainImages.size(); ++i)
	{
		//ubo
		VkDescriptorBufferInfo descriptorBufferInfo{};
		descriptorBufferInfo.buffer = m_vecSkyboxUniformBuffers[i];
		descriptorBufferInfo.offset = 0;
		descriptorBufferInfo.range = sizeof(SkyboxUniformBufferObject);

		VkWriteDescriptorSet uboWrite{};
		uboWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		uboWrite.dstSet = m_vecSkyboxDescriptorSets[i];
		uboWrite.dstBinding = 0;
		uboWrite.dstArrayElement = 0;
		uboWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboWrite.descriptorCount = 1;
		uboWrite.pBufferInfo = &descriptorBufferInfo;

		//cubemap sampler
		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = m_SkyboxTexture.m_ImageView;
		imageInfo.sampler = m_SkyboxTexture.m_Sampler;

		VkWriteDescriptorSet cubemapSamplerWrite{};
		cubemapSamplerWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		cubemapSamplerWrite.dstSet = m_vecSkyboxDescriptorSets[i];
		cubemapSamplerWrite.dstBinding = 1;
		cubemapSamplerWrite.dstArrayElement = 0;
		cubemapSamplerWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		cubemapSamplerWrite.descriptorCount = 1;
		cubemapSamplerWrite.pImageInfo = &imageInfo;

		std::vector<VkWriteDescriptorSet> vecDescriptorWrite = {
			uboWrite,
			cubemapSamplerWrite,
		};

		vkUpdateDescriptorSets(m_LogicalDevice, static_cast<UINT>(vecDescriptorWrite.size()), vecDescriptorWrite.data(), 0, nullptr);
	}
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

	if (m_bEnableSkybox)
	{
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_SkyboxGraphicPipeline);
		VkBuffer skyboxVertexBuffers[] = {
			m_SkyboxModel.m_VertexBuffer,
		};
		VkDeviceSize skyboxOffsets[]{ 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, skyboxVertexBuffers, skyboxOffsets);
		vkCmdBindIndexBuffer(commandBuffer, m_SkyboxModel.m_IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdBindDescriptorSets(commandBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			m_SkyboxGraphicPipelineLayout,
			0, 1,
			&m_vecSkyboxDescriptorSets[uiIdx],
			0, NULL);

		vkCmdDrawIndexed(commandBuffer, static_cast<UINT>(m_SkyboxModel.m_vecIndices.size()), 1, 0, 0, 0);
	}

	if (m_bEnableMeshGrid)
	{
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_MeshGridGraphicPipeline);
		VkBuffer meshGridVertexBuffers[] = {
			m_MeshGridVertexBuffer,
		};
		VkDeviceSize meshGridOffsets[]{ 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, meshGridVertexBuffers, meshGridOffsets);
		vkCmdBindIndexBuffer(commandBuffer, m_MeshGridIndexBuffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdBindDescriptorSets(commandBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			m_MeshGridGraphicPipelineLayout,
			0, 1,
			&m_vecMeshGridDescriptorSets[uiIdx],
			0, NULL);

		vkCmdDrawIndexed(commandBuffer, static_cast<UINT>(m_vecMeshGridIndices.size()), 1, 0, 0, 0);
		//vkCmdDraw(commandBuffer, static_cast<UINT>(m_vecMeshGridVertices.size()), 1, 0, 0);
	}

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

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicPipeline);
	VkBuffer vertexBuffers[] = {
		m_Model.m_VertexBuffer,
	};
	VkDeviceSize offsets[]{ 0 };
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

	vkCmdBindIndexBuffer(commandBuffer, m_Model.m_IndexBuffer, 0, VK_INDEX_TYPE_UINT32);

	for (UINT i = 0; i < INSTANCE_NUM; ++i)
	{
		UINT uiDynamicOffset = i * static_cast<UINT>(m_DynamicAlignment);

		vkCmdBindDescriptorSets(commandBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS, //descriptorSet����Pipeline���У������Ҫָ��������Graphic Pipeline����Compute Pipeline
			m_GraphicPipelineLayout, //PipelineLayout��ָ����descriptorSetLayout
			0,	//descriptorSet�����е�һ��Ԫ�ص��±� 
			1,	//descriptorSet������Ԫ�صĸ���
			&m_vecDescriptorSets[uiIdx],
			1, //���ö�̬Uniformƫ��
			&uiDynamicOffset	//ָ����̬Uniform��ƫ��
		);

		vkCmdDrawIndexed(commandBuffer, static_cast<UINT>(m_Model.m_vecIndices.size()), 1, 0, 0, 0);
	}

	vkCmdEndRenderPass(commandBuffer);

	if (ENABLE_GUI)
		g_UI.RecordRenderPass(uiIdx);

	VULKAN_ASSERT(vkEndCommandBuffer(commandBuffer), "End command buffer failed");
}

void VulkanRenderer::UpdateUniformBuffer(UINT uiIdx)
{
	static auto startTime = std::chrono::high_resolution_clock::now();
	auto currentTime = std::chrono::high_resolution_clock::now();

	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	m_UboData.view = m_Camera.GetViewMatrix();
	m_UboData.proj = m_Camera.GetProjMatrix();
	//OpenGL��Vulkan�Ĳ��� - Y�����Ƿ���
	m_UboData.proj[1][1] *= -1.f;

	void* uniformBufferData;
	vkMapMemory(m_LogicalDevice, m_vecUniformBufferMemories[uiIdx], 0, m_UboBufferSize, 0, &uniformBufferData);
	memcpy(uniformBufferData, &m_UboData, m_UboBufferSize);
	vkUnmapMemory(m_LogicalDevice, m_vecUniformBufferMemories[uiIdx]);

	glm::mat4* pModelMat = nullptr;
	float* pTextureIdx = nullptr;
	for (UINT i = 0; i < INSTANCE_NUM; ++i)
	{
		pModelMat = (glm::mat4*)((size_t)m_DynamicUboData.model + (i * m_DynamicAlignment));
		*pModelMat = glm::translate(glm::mat4(1.f), { ((float)i - (float)(INSTANCE_NUM / 2)) * m_fInstanceSpan, 0.f, 0.f });
		//*pModelMat = glm::rotate(glm::mat4(1.f), i * time * 1.f, { 0.f, 0.f, 1.f }) * *pModelMat;
		pTextureIdx = (float*)((size_t)m_DynamicUboData.fTextureIndex + (i * m_DynamicAlignment));
		*pTextureIdx = (float)(i % INSTANCE_NUM);
	}

	void* dynamicUniformBufferData;
	vkMapMemory(m_LogicalDevice, m_vecDynamicUniformBufferMemories[uiIdx], 0, m_DynamicUboBufferSize, 0, &dynamicUniformBufferData);
	memcpy(dynamicUniformBufferData, m_DynamicUboData.model, m_DynamicUboBufferSize);
	vkUnmapMemory(m_LogicalDevice, m_vecDynamicUniformBufferMemories[uiIdx]);
}

void VulkanRenderer::Render()
{
	static bool bNeedResize = false;
	
	if (ENABLE_GUI && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) && !ImGui::IsAnyItemActive())
		m_Camera.Tick();

	//�ȴ�fence��ֵ��Ϊsignaled
	vkWaitForFences(m_LogicalDevice, 1, &m_vecInFlightFences[m_uiCurFrameIdx], VK_TRUE, UINT64_MAX);

	if (bNeedResize)
	{
		WindowResize();
		bNeedResize = false;
	}

	if (ENABLE_GUI)
		g_UI.StartNewFrame();

	uint32_t uiImageIdx;
	VkResult res = vkAcquireNextImageKHR(m_LogicalDevice, m_SwapChain, UINT64_MAX,
		m_vecImageAvailableSemaphores[m_uiCurFrameIdx], VK_NULL_HANDLE, &uiImageIdx);
	if (res != VK_SUCCESS)
	{
		if (res == VK_ERROR_OUT_OF_DATE_KHR)
		{
			//SwapChain��WindowSurface�����ݣ��޷�������Ⱦ
			//һ�㷢����window�ߴ�ı�ʱ
			bNeedResize = true;
			return;
		}
		else if (res == VK_SUBOPTIMAL_KHR)
		{
			//SwapChain��Ȼ���ã�����WindowSurface��properties����ȫƥ��
		}
		else
		{
			ASSERT(false, "Accquire next swap chain image failed");
		}
	}

	//����fenceΪunsignaled
	vkResetFences(m_LogicalDevice, 1, &m_vecInFlightFences[m_uiCurFrameIdx]);

	vkResetCommandBuffer(m_vecCommandBuffers[m_uiCurFrameIdx], 0);

	UpdateUniformBuffer(m_uiCurFrameIdx);

	if (m_bEnableSkybox)
		UpdateSkyboxUniformBuffer(m_uiCurFrameIdx);

	if (m_bEnableMeshGrid)
		UpdateMeshGridUniformBuffer(m_uiCurFrameIdx);

	RecordCommandBuffer(m_vecCommandBuffers[m_uiCurFrameIdx], uiImageIdx);

	//auto uiCommandBuffer = g_UI.FillCommandBuffer(m_uiCurFrameIdx);

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
		//uiCommandBuffer,
	};
	submitInfo.commandBufferCount = static_cast<UINT>(commandBuffers.size());
	submitInfo.pCommandBuffers = commandBuffers.data();

	VkSemaphore signalSemaphore[] = {
		m_vecRenderFinishedSemaphores[m_uiCurFrameIdx],
	};
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphore;

	//submit֮�󣬻Ὣfence��Ϊsignaled
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
			bNeedResize = true;
			m_bFrameBufferResized = false;
		}
		else
		{
			throw std::runtime_error("Present Swap Chain Image To Queue Failed");
		}
	}

	m_uiFrameCounter++;

	m_uiCurFrameIdx = (m_uiCurFrameIdx + 1) % static_cast<UINT>(m_vecSwapChainImages.size());
}

void VulkanRenderer::CleanWindowResizeResource()
{
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
		printf("Destroy Old ImageView:%p\n", imageView);
	}
	
	for (auto& image : m_vecSwapChainImages)
	{
		printf("Destroy Old Image:%p\n", image);
	}

	vkFreeCommandBuffers(m_LogicalDevice, m_CommandPool, m_vecCommandBuffers.size(), m_vecCommandBuffers.data());

	//����SwapChain
	vkDestroySwapchainKHR(m_LogicalDevice, m_SwapChain, nullptr);
	vkDestroyPipeline(m_LogicalDevice, m_GraphicPipeline, nullptr);

	if (ENABLE_GUI)
		g_UI.CleanResizeResource();
}

void VulkanRenderer::RecreateWindowResizeResource()
{
	CreateSwapChain();

	CreateDepthImage();
	CreateDepthImageView();

	CreateSwapChainImages();
	for (auto& image : m_vecSwapChainImages)
	{
		printf("Create New Image:%p\n", image);
	}
	CreateSwapChainImageViews();
	for (auto& imageView : m_vecSwapChainImageViews)
	{
		printf("Create New ImageView:%p\n", imageView);
	}
	CreateSwapChainFrameBuffers();

	CreateCommandBuffer();

	CreateGraphicPipeline();

	if (ENABLE_GUI)
		g_UI.RecreateResizeResource();
}

void VulkanRenderer::WindowResize()
{
	//���⴦������С�������
	int nWidth = 0;
	int nHeight = 0;
	glfwGetFramebufferSize(m_pWindow, &nWidth, &nHeight);
	while (nWidth == 0 || nHeight == 0)
	{
		glfwGetFramebufferSize(m_pWindow, &nWidth, &nHeight);
		glfwWaitEvents();
	}

	m_Camera.SetViewportSize(nWidth, nHeight);

	//��Ҫ�ؽ�����Դ��
	//1. �ʹ��ڴ�С��صģ�Depth(Image, Memory, ImageView)��SwapChain Image��Viewport/Stencil
	//2. ������DepthImageView�ģ�FrameBuffer
	//3. ������SwapChain Image�ģ�FrameBuffer
	//4. ������FrameBuffer�ģ�CommandBuffer(RenderPassBeginInfo)
	//5. ������ViewportStencil�ģ�Pipeline

	//�ȴ���Դ����ռ��
	vkDeviceWaitIdle(m_LogicalDevice);
	CleanWindowResizeResource();

	vkDeviceWaitIdle(m_LogicalDevice);
	RecreateWindowResizeResource();
}

void VulkanRenderer::LoadTexture(const std::filesystem::path& filepath, DZW_VulkanWrap::Texture& texture)
{
	texture.m_Filepath = filepath;

	if (texture.IsKtxTexture())
	{
		ktxResult result;
		ktxTexture* ktxTexture;
		result = ktxTexture_CreateFromNamedFile(texture.m_Filepath.string().c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTexture);
		ASSERT(result == KTX_SUCCESS, std::format("KTX load image {} failed", texture.m_Filepath.string()));

		ASSERT(ktxTexture->glFormat == GL_RGBA);
		ASSERT(ktxTexture->numDimensions == 2);

		ktx_uint8_t* ktxTextureData = ktxTexture_GetData(ktxTexture);
		texture.m_Size = ktxTexture_GetSize(ktxTexture);

		texture.m_uiWidth = ktxTexture->baseWidth;
		texture.m_uiHeight = ktxTexture->baseHeight;
		texture.m_uiMipLevelNum = ktxTexture->numLevels;
		texture.m_uiLayerNum = ktxTexture->numLayers;
		texture.m_uiFaceNum = ktxTexture->numFaces;

		if (texture.IsTextureArray())
		{
			auto& physicalDeviceInfo = m_mapPhysicalDeviceInfo.at(m_PhysicalDevice);
			UINT uiMaxLayerNum = physicalDeviceInfo.properties.limits.maxImageArrayLayers;
			ASSERT(texture.m_uiLayerNum <= uiMaxLayerNum, std::format("TextureArray {} layout count {} exceed max limit {}", texture.m_Filepath.string(), texture.m_uiLayerNum, uiMaxLayerNum));
		}

		CreateImageAndBindMemory(texture.m_uiWidth, texture.m_uiHeight, texture.m_uiMipLevelNum, texture.m_uiLayerNum, texture.m_uiFaceNum,
			VK_SAMPLE_COUNT_1_BIT,
			VK_FORMAT_R8G8B8A8_SRGB,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			texture.m_Image, texture.m_Memory);

		//copy֮ǰ����layout�ӳ�ʼ��undefinedתΪtransfer dst
		ChangeImageLayout(texture.m_Image,
			VK_FORMAT_R8G8B8A8_SRGB,				//image format
			texture.m_uiMipLevelNum,				//mipmap levels
			texture.m_uiLayerNum,					//layers
			texture.m_uiFaceNum,					//faces
			VK_IMAGE_LAYOUT_UNDEFINED,				//src layout
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);	//dst layout

		TransferImageDataByStageBuffer(ktxTextureData, texture.m_Size, texture.m_Image, texture.m_uiWidth, texture.m_uiHeight, texture, ktxTexture);

		ktxTexture_Destroy(ktxTexture);
	}
	else
	{
		//stb����һ����������ͼ����⣬�޷�ֱ�Ӷ�ȡͼƬ��mipmap�㼶
		int nTexWidth = 0;
		int nTexHeight = 0;
		int nTexChannel = 0;
		stbi_uc* pixels = stbi_load(texture.m_Filepath.string().c_str(), &nTexWidth, &nTexHeight, &nTexChannel, STBI_rgb_alpha);
		ASSERT(pixels, std::format("STB load image {} failed", texture.m_Filepath.string()));

		ASSERT(nTexChannel == 4);

		texture.m_uiWidth = static_cast<UINT>(nTexWidth);
		texture.m_uiHeight = static_cast<UINT>(nTexHeight);
		texture.m_uiMipLevelNum = 1;
		texture.m_uiLayerNum = 1;
		texture.m_uiFaceNum = 1;

		texture.m_Size = texture.m_uiWidth * texture.m_uiHeight * static_cast<UINT>(nTexChannel);

		CreateImageAndBindMemory(texture.m_uiWidth, texture.m_uiHeight, 
			texture.m_uiMipLevelNum, texture.m_uiLayerNum, texture.m_uiFaceNum, 
			VK_SAMPLE_COUNT_1_BIT,
			VK_FORMAT_R8G8B8A8_SRGB,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			texture.m_Image, texture.m_Memory);

		//copy֮ǰ����layout�ӳ�ʼ��undefinedתΪtransfer dst
		ChangeImageLayout(texture.m_Image,
			VK_FORMAT_R8G8B8A8_SRGB,				//image format
			texture.m_uiMipLevelNum,				//mipmap levels
			texture.m_uiLayerNum,					//layers
			texture.m_uiFaceNum,					//faces
			VK_IMAGE_LAYOUT_UNDEFINED,				//src layout
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);	//dst layout

		TransferImageDataByStageBuffer(pixels, texture.m_Size, texture.m_Image, texture.m_uiWidth, texture.m_uiHeight, texture, nullptr);

		stbi_image_free(pixels);

	}

	//transfer֮�󣬽�layout��transfer dstתΪshader readonly
	//���ʹ��mipmap��֮����generateMipmaps�н�layoutתΪshader readonly

	ChangeImageLayout(texture.m_Image,
		VK_FORMAT_R8G8B8A8_SRGB,
		texture.m_uiMipLevelNum,
		texture.m_uiLayerNum,
		texture.m_uiFaceNum,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	
	//generateMipmaps(m_TextureImage, VK_FORMAT_R8G8B8A8_SRGB, nTexWidth, nTexHeight, m_uiMipmapLevel);

	texture.m_ImageView = CreateImageView(texture.m_Image, 
		VK_FORMAT_R8G8B8A8_SRGB,	//��ʽΪsRGB
		VK_IMAGE_ASPECT_COLOR_BIT,	//aspectFlagsΪCOLOR_BIT
		texture.m_uiMipLevelNum,
		texture.m_uiLayerNum,
		texture.m_uiFaceNum
	);

	VkSamplerCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	//���ù�������Ƿ����ʱ�Ĳ���������������nearest��linear��cubic��
	createInfo.magFilter = VK_FILTER_LINEAR;
	createInfo.minFilter = VK_FILTER_LINEAR;

	//����������������߽�ʱ��Ѱַģʽ��������repeat��mirror��clamp to edge��clamp to border��
	createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

	//�����Ƿ����������Թ��ˣ����Ӳ����һ��֧��Anisotropy����Ҫȷ��Ӳ��֧�ָ�preperty
	createInfo.anisotropyEnable = VK_FALSE;
	if (createInfo.anisotropyEnable == VK_TRUE)
	{
		auto& physicalDeviceProperties = m_mapPhysicalDeviceInfo.at(m_PhysicalDevice).properties;
		createInfo.maxAnisotropy = physicalDeviceProperties.limits.maxSamplerAnisotropy;
	}

	//����ѰַģʽΪclamp to borderʱ�������ɫ
	createInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

	//���Ϊtrue��������Ϊ[0, texWidth), [0, texHeight)
	//���Ϊfalse��������Ϊ��ͳ��[0, 1), [0, 1)
	createInfo.unnormalizedCoordinates = VK_FALSE;

	//�����Ƿ����Ƚ���ԱȽϽ���Ĳ�����ͨ�����ڰٷֱ��ڽ��˲���Shadow Map PCS��
	createInfo.compareEnable = VK_FALSE;
	createInfo.compareOp = VK_COMPARE_OP_ALWAYS;

	//����mipmap��ز���
	createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	createInfo.mipLodBias = 0.f;
	createInfo.minLod = 0.f;
	createInfo.maxLod = static_cast<float>(m_Texture.m_uiMipLevelNum);

	VULKAN_ASSERT(vkCreateSampler(m_LogicalDevice, &createInfo, nullptr, &texture.m_Sampler), "Create texture sampler failed");
}

void VulkanRenderer::FreeTexture(DZW_VulkanWrap::Texture& texture)
{
	vkDestroySampler(m_LogicalDevice, texture.m_Sampler, nullptr);

	vkDestroyImageView(m_LogicalDevice, texture.m_ImageView, nullptr);
	vkDestroyImage(m_LogicalDevice, texture.m_Image, nullptr);
	vkFreeMemory(m_LogicalDevice, texture.m_Memory, nullptr);
}

void VulkanRenderer::LoadOBJ(DZW_VulkanWrap::Model& model)
{
	tinyobj::attrib_t attr;	//�洢���ж��㡢���ߡ�UV����
	std::vector<tinyobj::shape_t> vecShapes;
	std::vector<tinyobj::material_t> vecMaterials;
	std::string strWarning;
	std::string strError;

	bool res = tinyobj::LoadObj(&attr, &vecShapes, &vecMaterials, &strWarning, &strError, model.m_Filepath.string().c_str());
	ASSERT(res, std::format("Load obj file {} failed", model.m_Filepath.string().c_str()));

	model.m_vecVertices.clear();
	model.m_vecIndices.clear();

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

			model.m_vecVertices.push_back(vert);
			model.m_vecIndices.push_back(static_cast<uint32_t>(model.m_vecIndices.size()));
		}
	}
}

void VulkanRenderer::LoadGLTF(DZW_VulkanWrap::Model& model)
{
	tinygltf::Model gltfModel;
	tinygltf::TinyGLTF gltfLoader;

	std::string err;
	std::string warn;
	bool ret = false;

	if (model.m_Filepath.extension().string() == ".glb")
		ret = gltfLoader.LoadBinaryFromFile(&gltfModel, &err, &warn, model.m_Filepath.string());
	else if (model.m_Filepath.extension().string() == ".gltf")
		ret = gltfLoader.LoadASCIIFromFile(&gltfModel, &err, &warn, model.m_Filepath.string());

	if (!warn.empty())
		Log::Warn(std::format("Load gltf warn: {}", warn.c_str()));

	if (!err.empty())
		Log::Error(std::format("Load gltf error: {}", err.c_str()));

	ASSERT(ret, "Load gltf failed");

	model.m_vecVertices.clear();
	model.m_vecIndices.clear();

	for (size_t i = 0; i < gltfModel.scenes.size(); ++i)
	{
		DZW_VulkanWrap::Model::Scene scene;
		const tinygltf::Scene& gltfScene = gltfModel.scenes[i];
		for (size_t j = 0; j < gltfScene.nodes.size(); ++j)
		{
			DZW_VulkanWrap::Model::Node node;
			node.nIndex = gltfScene.nodes[j];

			const tinygltf::Node& gltfNode = gltfModel.nodes[node.nIndex];
			DZW_VulkanWrap::Model::Mesh mesh;
			mesh.nIndex = gltfNode.mesh;
			if (mesh.nIndex >= 0)
			{
				const tinygltf::Mesh& gltfMesh = gltfModel.meshes[mesh.nIndex];
				for (size_t k = 0; k < gltfMesh.primitives.size(); ++k)
				{
					uint32_t vertexStart = static_cast<uint32_t>(model.m_vecVertices.size());

					DZW_VulkanWrap::Model::Primitive primitive;
					primitive.uiFirstIndex = static_cast<uint32_t>(model.m_vecIndices.size());


					const tinygltf::Primitive& gltfPrimitive = gltfMesh.primitives[k];

					//��ȡvertex����
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

					//��ȡIndex����
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

					mesh.vecPrimitives.push_back(primitive);
				}
			}

			node.mesh = mesh;
			scene.vecNodes.push_back(node);
		}
		model.m_vecScenes.push_back(scene);
	}
}

void VulkanRenderer::LoadModel(const std::filesystem::path& filepath, DZW_VulkanWrap::Model& model)
{
	model.m_Filepath = filepath;

	if (model.IsGLTF())
		LoadGLTF(model);
	else if (model.IsOBJ())
		LoadOBJ(model);
	else
		ASSERT(false, "Unsupport model type");

	Log::Info(std::format("Vertex count : {}", model.m_vecVertices.size()));
	ASSERT(model.m_vecVertices.size() > 0, "Vertex data empty");
	VkDeviceSize verticesSize = sizeof(model.m_vecVertices[0]) * model.m_vecVertices.size();
	CreateBufferAndBindMemory(verticesSize,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		model.m_VertexBuffer, model.m_VertexBufferMemory);
	TransferBufferDataByStageBuffer(model.m_vecVertices.data(), verticesSize, model.m_VertexBuffer);


	Log::Info(std::format("Index count : {}", model.m_vecIndices.size()));
	VkDeviceSize indicesSize = sizeof(model.m_vecIndices[0]) * model.m_vecIndices.size();
	if (indicesSize > 0)
	CreateBufferAndBindMemory(indicesSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		model.m_IndexBuffer, model.m_IndexBufferMemory);
	TransferBufferDataByStageBuffer(model.m_vecIndices.data(), indicesSize, model.m_IndexBuffer);
}

void VulkanRenderer::FreeModel(DZW_VulkanWrap::Model& model)
{
	vkFreeMemory(m_LogicalDevice, model.m_VertexBufferMemory, nullptr);
	vkDestroyBuffer(m_LogicalDevice, model.m_VertexBuffer, nullptr);

	if (model.m_vecIndices.size() > 0)
	{
		vkFreeMemory(m_LogicalDevice, model.m_IndexBufferMemory, nullptr);
		vkDestroyBuffer(m_LogicalDevice, model.m_IndexBuffer, nullptr);
	}
}



