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

#include "json.hpp"

#define INSTANCE_NUM 9

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

	m_strWindowTitle = "Vulkan Renderer";

	m_bFrameBufferResized = false;

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
	/*******************必要资源*******************/

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
	CreateCommandBuffers();
	
	CreateSyncObjects();

	SetupCamera();

	g_UI.Init(this);



	/*******************独立资源*******************/

	//OBJ Model
	CreateCommonShader();
	CreateCommonMVPUniformBufferAndMemory();

	CreateCommonDescriptorSetLayout();
	CreateCommonDescriptorPool();

	CreateCommonGraphicPipelineLayout();
	CreateCommonGraphicPipeline();

	m_testObjModel = DZW_VulkanWrap::ModelFactor::CreateModel(this, "./Assert/Model/samplescene.obj");

	//glTF Model
	CreateGLTFShader();

	CreateGLTFDescriptorSetLayout();
	CreateGLTFDescriptorPool();

	CreateGLTFGraphicPipelineLayout();
	CreateGLTFGraphicPipeline();

	m_testGLTFModel = DZW_VulkanWrap::ModelFactor::CreateModel(this, "./Assert/Model/samplescene.gltf");
	
	//LoadPlanetInfo();

	//LoadModel("./Assert/Model/sphere.obj", m_Model);
	//LoadTexture("./Assert/Texture/solarsystem_array_rgba8.ktx", m_Texture);
	//CreateShader();
	//CreateUniformBuffers();
	//CreateDescriptorSetLayout();
	//CreateDescriptorPool();
	//CreateDescriptorSets();
	//CreateGraphicPipelineLayout();
	//CreateGraphicPipeline();

	//Skybox
	m_SkyboxModel = DZW_VulkanWrap::ModelFactor::CreateModel(this, "./Assert/Model/Skybox/cube.gltf");
	m_SkyboxTexture = DZW_VulkanWrap::TextureFactor::CreateTexture(this, "./Assert/Texture/Skybox/milkyway_cubemap.ktx");
	CreateSkyboxShader();
	CreateSkyboxUniformBuffers();
	CreateSkyboxDescriptorSetLayout();
	CreateSkyboxDescriptorPool();
	CreateSkyboxDescriptorSets();
	CreateSkyboxGraphicPipelineLayout();
	CreateSkyboxGraphicPipeline();

	//Mesh Grid
	//CreateMeshGridVertexBuffer();
	//CreateMeshGridIndexBuffer();
	//CreateMeshGridShader();
	//CreateMeshGridUniformBuffers();
	//CreateMeshGridDescriptorSetLayout();
	//CreateMeshGridDescriptorPool();
	//CreateMeshGridDescriptorSets();
	//CreateMeshGridGraphicPipelineLayout();
	//CreateMeshGridGraphicPipeline();

	////Ellipse
	//CreateEllipseVertexBuffer();
	//CreateEllipseIndexBuffer();
	//CreateEllipseUniformBuffers();
	//CreateEllipseDescriptorSetLayout();
	//CreateEllipseDescriptorPool();
	//CreateEllipseDescriptorSets();
	//CreateEllipseGraphicPipelineLayout();
	//CreateEllipseGraphicPipeline();

	////BlinnPhong
	//InitBlinnPhongLightMaterialInfo();
	//LoadModel("./Assert/Model/sphere.obj", m_BlinnPhongModel);
	//CreateBlinnPhongShaderModule();
	//CreateBlinnPhongMVPUniformBuffers();
	//CreateBlinnPhongLightUniformBuffers();
	//CreateBlinnPhongMaterialUniformBuffers();
	//CreateBlinnPhongDescriptorSetLayout();
	//CreateBlinnPhongDescriptorPool();
	//CreateBlinnPhongDescriptorSets();
	//CreateBlinnPhongGraphicPipelineLayout();
	//CreateBlinnPhongGraphicPipeline();

	////PBR
	//InitPBRLightMaterialInfo();
	//LoadModel("./Assert/Model/sphere.obj", m_PBRModel);
	//CreatePBRShaderModule();
	//CreatePBRMVPUniformBuffers();
	//CreatePBRLightUniformBuffers();
	//CreatePBRMaterialUniformBuffers();
	//CreatePBRDescriptorSetLayout();
	//CreatePBRDescriptorPool();
	//CreatePBRDescriptorSets();
	//CreatePBRGraphicPipelineLayout();
	//CreatePBRGraphicPipeline();
}

void VulkanRenderer::Loop()
{
	while (!glfwWindowShouldClose(m_pWindow))
	{
		static std::chrono::time_point<std::chrono::high_resolution_clock> lastTimestamp = std::chrono::high_resolution_clock::now();
		
		glfwPollEvents();

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

	//Skybox
	m_SkyboxModel.reset();
	m_SkyboxTexture.reset();

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

	////Mesh Grid
	//vkDestroyDescriptorPool(m_LogicalDevice, m_MeshGridDescriptorPool, nullptr);
	//vkDestroyDescriptorSetLayout(m_LogicalDevice, m_MeshGridDescriptorSetLayout, nullptr);
	//for (size_t i = 0; i < m_vecSwapChainImages.size(); ++i)
	//{
	//	vkFreeMemory(m_LogicalDevice, m_vecMeshGridUniformBufferMemories[i], nullptr);
	//	vkDestroyBuffer(m_LogicalDevice, m_vecMeshGridUniformBuffers[i], nullptr);
	//}

	//for (const auto& shaderModule : m_mapMeshGridShaderModule)
	//{
	//	vkDestroyShaderModule(m_LogicalDevice, shaderModule.second, nullptr);
	//}

	//vkDestroyPipeline(m_LogicalDevice, m_MeshGridGraphicPipeline, nullptr);
	//vkDestroyPipelineLayout(m_LogicalDevice, m_MeshGridGraphicPipelineLayout, nullptr);

	//vkDestroyBuffer(m_LogicalDevice, m_MeshGridVertexBuffer, nullptr);
	//vkFreeMemory(m_LogicalDevice, m_MeshGridVertexBufferMemory, nullptr);

	//vkDestroyBuffer(m_LogicalDevice, m_MeshGridIndexBuffer, nullptr);
	//vkFreeMemory(m_LogicalDevice, m_MeshGridIndexBufferMemory, nullptr);

	////Blinn Phong
	//FreeModel(m_BlinnPhongModel);

	//for (const auto& shaderModule : m_mapBlinnPhongShaderModule)
	//{
	//	vkDestroyShaderModule(m_LogicalDevice, shaderModule.second, nullptr);
	//}
	//for (size_t i = 0; i < m_vecSwapChainImages.size(); ++i)
	//{
	//	vkFreeMemory(m_LogicalDevice, m_vecBlinnPhongMVPUniformBufferMemories[i], nullptr);
	//	vkDestroyBuffer(m_LogicalDevice, m_vecBlinnPhongMVPUniformBuffers[i], nullptr);

	//	vkFreeMemory(m_LogicalDevice, m_vecBlinnPhongLightUniformBufferMemories[i], nullptr);
	//	vkDestroyBuffer(m_LogicalDevice, m_vecBlinnPhongLightUniformBuffers[i], nullptr);

	//	vkFreeMemory(m_LogicalDevice, m_vecBlinnPhongMaterialUniformBufferMemories[i], nullptr);
	//	vkDestroyBuffer(m_LogicalDevice, m_vecBlinnPhongMaterialUniformBuffers[i], nullptr);
	//}

	//vkDestroyDescriptorPool(m_LogicalDevice, m_BlinnPhongDescriptorPool, nullptr);
	//vkDestroyDescriptorSetLayout(m_LogicalDevice, m_BlinnPhongDescriptorSetLayout, nullptr);

	//vkDestroyPipeline(m_LogicalDevice, m_BlinnPhongGraphicPipeline, nullptr);
	//vkDestroyPipelineLayout(m_LogicalDevice, m_BlinnPhongGraphicPipelineLayout, nullptr);


	////PBR
	//FreeModel(m_PBRModel);

	//for (const auto& shaderModule : m_mapPBRShaderModule)
	//{
	//	vkDestroyShaderModule(m_LogicalDevice, shaderModule.second, nullptr);
	//}
	//for (size_t i = 0; i < m_vecSwapChainImages.size(); ++i)
	//{
	//	vkFreeMemory(m_LogicalDevice, m_vecPBRMVPUniformBufferMemories[i], nullptr);
	//	vkDestroyBuffer(m_LogicalDevice, m_vecPBRMVPUniformBuffers[i], nullptr);

	//	vkFreeMemory(m_LogicalDevice, m_vecPBRLightUniformBufferMemories[i], nullptr);
	//	vkDestroyBuffer(m_LogicalDevice, m_vecPBRLightUniformBuffers[i], nullptr);

	//	vkFreeMemory(m_LogicalDevice, m_vecPBRMaterialUniformBufferMemories[i], nullptr);
	//	vkDestroyBuffer(m_LogicalDevice, m_vecPBRMaterialUniformBuffers[i], nullptr);
	//}

	//vkDestroyDescriptorPool(m_LogicalDevice, m_PBRDescriptorPool, nullptr);
	//vkDestroyDescriptorSetLayout(m_LogicalDevice, m_PBRDescriptorSetLayout, nullptr);

	//vkDestroyPipeline(m_LogicalDevice, m_PBRGraphicPipeline, nullptr);
	//vkDestroyPipelineLayout(m_LogicalDevice, m_PBRGraphicPipelineLayout, nullptr);

	//----------------------------------------------------------------------------
	for (const auto& shaderModule : m_mapCommonShaderModule)
	{
		vkDestroyShaderModule(m_LogicalDevice, shaderModule.second, nullptr);
	}

	vkFreeMemory(m_LogicalDevice, m_CommonMVPUniformBufferMemory, nullptr);
	vkDestroyBuffer(m_LogicalDevice, m_CommonMVPUniformBuffer, nullptr);


	vkDestroyDescriptorPool(m_LogicalDevice, m_CommonDescriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(m_LogicalDevice, m_CommonDescriptorSetLayout, nullptr);

	vkDestroyPipeline(m_LogicalDevice, m_CommonGraphicPipeline, nullptr);
	vkDestroyPipelineLayout(m_LogicalDevice, m_CommonGraphicPipelineLayout, nullptr);
	vkDestroyPipelineCache(m_LogicalDevice, m_CommonGraphicPipelineCache, nullptr);

	m_testObjModel.reset();

	//----------------------------------------------------------------------------

	for (const auto& shaderModule : m_mapGLTFShaderModule)
	{
		vkDestroyShaderModule(m_LogicalDevice, shaderModule.second, nullptr);
	}

	vkDestroyDescriptorPool(m_LogicalDevice, m_GLTFDescriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(m_LogicalDevice, m_GLTFDescriptorSetLayout, nullptr);

	vkDestroyPipeline(m_LogicalDevice, m_GLTFGraphicPipeline, nullptr);
	vkDestroyPipelineLayout(m_LogicalDevice, m_GLTFGraphicPipelineLayout, nullptr);

	m_testGLTFModel.reset();

	//----------------------------------------------------------------------------
	//_aligned_free(m_DynamicUboData.model);

	//FreeTexture(m_Texture);

	//for (const auto& shaderModule : m_mapShaderModule)
	//{
	//	vkDestroyShaderModule(m_LogicalDevice, shaderModule.second, nullptr);
	//}

	//vkDestroyPipeline(m_LogicalDevice, m_GraphicPipeline, nullptr);
	//vkDestroyPipelineLayout(m_LogicalDevice, m_GraphicPipelineLayout, nullptr);


	//vkDestroyDescriptorPool(m_LogicalDevice, m_DescriptorPool, nullptr);
	//vkDestroyDescriptorSetLayout(m_LogicalDevice, m_DescriptorSetLayout, nullptr);
	//for (size_t i = 0; i < m_vecSwapChainImages.size(); ++i)
	//{
	//	vkFreeMemory(m_LogicalDevice, m_vecUniformBufferMemories[i], nullptr);
	//	vkDestroyBuffer(m_LogicalDevice, m_vecUniformBuffers[i], nullptr);

	//	vkFreeMemory(m_LogicalDevice, m_vecDynamicUniformBufferMemories[i], nullptr);
	//	vkDestroyBuffer(m_LogicalDevice, m_vecDynamicUniformBuffers[i], nullptr);
	//}

	/********************************************************************/

	//清理SwapChain会自动释放其下的Image
	vkDestroySwapchainKHR(m_LogicalDevice, m_SwapChain, nullptr);

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

void VulkanRenderer::MouseButtonCallBack(GLFWwindow* pWindow, int nButton, int nAction, int nMods)
{
	auto vulkanRenderer = reinterpret_cast<VulkanRenderer*>(glfwGetWindowUserPointer(pWindow));
	if (vulkanRenderer)
	{
		if (nButton == GLFW_MOUSE_BUTTON_LEFT && nAction == GLFW_PRESS)
		{
			//vulkanRenderer->m_fMeshGridSplit++;
			//vulkanRenderer->RecreateMeshGrid();
		}
	}
}

void VulkanRenderer::InitWindow()
{
	glfwInit();
	ASSERT(glfwVulkanSupported(), "GLFW version not support vulkan");

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());

	m_uiPrimaryMonitorWidth = mode->width;
	m_uiPrimaryMonitorHeight = mode->height;

	m_uiWindowWidth = m_uiPrimaryMonitorWidth * 0.8;
	m_uiWindowHeight = m_uiPrimaryMonitorHeight * 0.8;

	m_pWindow = glfwCreateWindow(m_uiWindowWidth, m_uiWindowHeight, m_strWindowTitle.c_str(), nullptr, nullptr);
	glfwMakeContextCurrent(m_pWindow);

	glfwSetWindowUserPointer(m_pWindow, this);
	glfwSetFramebufferSizeCallback(m_pWindow, FrameBufferResizeCallBack);

	glfwSetMouseButtonCallback(m_pWindow, MouseButtonCallBack);
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
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT/* |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
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
	deviceFeatures.fillModeNonSolid = VK_TRUE;
	deviceFeatures.wideLines = VK_TRUE;
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
	VkSwapchainKHR oldSwapChain = m_SwapChain;
	m_SwapChain = VK_NULL_HANDLE;

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
	//Vulkan会尝试重用旧交换链中的一些可能还有效的资源，以此提高性能和效率
	//还可以保证在新的交换链创建完成之前旧的交换链仍可使用，避免重建期间出现闪烁
	createInfo.oldSwapchain = oldSwapChain;

	VULKAN_ASSERT(vkCreateSwapchainKHR(m_LogicalDevice, &createInfo, nullptr, &m_SwapChain), "Create swap chain failed");

	if (oldSwapChain != VK_NULL_HANDLE)
	{
		vkDestroySwapchainKHR(m_LogicalDevice, oldSwapChain, nullptr);
		oldSwapChain = VK_NULL_HANDLE;
	}
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
	attachmentDescriptions[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

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

void VulkanRenderer::CreateShader()
{
	m_mapShaderModule.clear();

	ASSERT(m_mapShaderPath.size() > 0, "Detect no shader spv file");

	for (const auto& spvPath : m_mapShaderPath)
	{
		auto shaderModule = DZW_VulkanUtils::CreateShaderModule(m_LogicalDevice, DZW_VulkanUtils::ReadShaderFile(spvPath.second));

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
	imageCreateInfo.arrayLayers = (uiFaceCount == 6) ? uiFaceCount : uiLayerCount; // Cubemap的face数在vulkan中用作arrayLayers
	imageCreateInfo.format = format;
	imageCreateInfo.tiling = tiling;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCreateInfo.usage = usage;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.samples = sampleCount;
	imageCreateInfo.flags = (uiFaceCount == 6) ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0; //Cubemap需要设置对应flags

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
	barrier.subresourceRange.levelCount = uiMipLevelCount;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = (uiFaceCount == 6) ? uiFaceCount : uiLayerCount;
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

void VulkanRenderer::TransferImageDataByStageBuffer(const void* pData, VkDeviceSize imageSize, VkImage& image, UINT uiWidth, UINT uiHeight)
{
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

void VulkanRenderer::CreateDescriptorSetLayout()
{
	//UniformBufferObject Binding
	VkDescriptorSetLayoutBinding uboLayoutBinding{};
	uboLayoutBinding.binding = 0; //对应Vertex Shader中的layout binding
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; //只需要在vertex stage生效
	uboLayoutBinding.pImmutableSamplers = nullptr;

	//Dynamic UniformBufferObject Binding
	VkDescriptorSetLayoutBinding dynamicUboLayoutBinding{};
	dynamicUboLayoutBinding.binding = 1; //对应Vertex Shader中的layout binding
	dynamicUboLayoutBinding.descriptorCount = 1;
	dynamicUboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	dynamicUboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; //只需要在vertex stage生效
	dynamicUboLayoutBinding.pImmutableSamplers = nullptr;

	//CombinedImageSampler Binding
	VkDescriptorSetLayoutBinding samplerLayoutBinding{};
	samplerLayoutBinding.binding = 2; ////对应Fragment Shader中的layout binding
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; //只用于fragment stage
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
		//imageInfo.imageView = m_Texture.m_ImageView;
		//imageInfo.sampler = m_Texture.m_Sampler;

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

void VulkanRenderer::CreateCommandBuffers()
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

	//VULKAN_ASSERT(vkCreatePipelineLayout(m_LogicalDevice, &pipelineLayoutCreateInfo, nullptr, &m_GraphicPipelineLayout), "Create pipeline layout failed");
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

	std::vector<VkDynamicState> vecDynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};
	dynamicStateCreateInfo.dynamicStateCount = static_cast<UINT>(vecDynamicStates.size());
	dynamicStateCreateInfo.pDynamicStates = vecDynamicStates.data();


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

	//-----------------------Raserization State--------------------------//
	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo{};
	rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;	//开启后，超过远近平面的部分会被截断在远近平面上，而不是丢弃
	rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;	//开启后，禁止所有图元经过光栅化器
	rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;	//图元模式，可以是FILL、LINE、POINT
	rasterizationStateCreateInfo.lineWidth = 1.f;	//指定光栅化后的线段宽度
	rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;	//剔除模式，可以是NONE、FRONT、BACK、FRONT_AND_BACK
	rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; //顶点序，可以是顺时针cw或逆时针ccw
	rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE; //深度偏移，一般用于Shaodw Map中避免阴影痤疮
	rasterizationStateCreateInfo.depthBiasConstantFactor = 0.f;
	rasterizationStateCreateInfo.depthBiasClamp = 0.f;
	rasterizationStateCreateInfo.depthBiasSlopeFactor = 0.f;

	//-----------------------Multisample State--------------------------//
	VkPipelineMultisampleStateCreateInfo multisamplingStateCreateInfo{};
	multisamplingStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	//multisamplingStateCreateInfo.sampleShadingEnable = (VkBool32)(m_Texture.m_uiMipLevelNum > 1);
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
	//VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
	//pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	//pipelineCreateInfo.stageCount = static_cast<UINT>(m_mapShaderModule.size());
	//pipelineCreateInfo.pStages = shaderStageCreateInfos;
	//pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
	//pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
	//pipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
	//pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	//pipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
	//pipelineCreateInfo.pMultisampleState = &multisamplingStateCreateInfo;
	//pipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
	//pipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
	//pipelineCreateInfo.layout = m_GraphicPipelineLayout;
	//pipelineCreateInfo.renderPass = m_RenderPass;
	//pipelineCreateInfo.subpass = 0;
	//pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	//pipelineCreateInfo.basePipelineIndex = -1;

	////创建管线时，如果提供了VkPipelineCache对象，Vulkan会尝试从中重用数据
	////如果没有可重用的数据，新的数据会被添加到缓存中
	//VkPipelineCache pipelineCache = VK_NULL_HANDLE;

	//VULKAN_ASSERT(vkCreateGraphicsPipelines(m_LogicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &m_GraphicPipeline), "Create graphic pipeline failed");
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
	m_Camera.Init(45.f, static_cast<float>(m_SwapChainExtent2D.width), static_cast<float>(m_SwapChainExtent2D.height), 0.1f, 10000.f,
		m_pWindow, { 0.f, 0.f, -10.f }, { 0.f, 0.f, 0.f }, true);

	glfwSetWindowUserPointer(m_pWindow, (void*)&m_Camera);

	glfwSetScrollCallback(m_pWindow, [](GLFWwindow* window, double dOffsetX, double dOffsetY)
		{
			if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
				return;
			auto& camera = *(Camera*)glfwGetWindowUserPointer(window);
			camera.OnMouseScroll(dOffsetX, dOffsetY);
		}
	);
	glfwSetKeyCallback(m_pWindow, [](GLFWwindow* window, int nKey, int nScanmode, int nAction, int Mods)
		{
			if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
				return;
			auto& camera = *(Camera*)glfwGetWindowUserPointer(window);
			camera.OnKeyPress(nKey);
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
		auto shaderModule = DZW_VulkanUtils::CreateShaderModule(m_LogicalDevice, DZW_VulkanUtils::ReadShaderFile(spvPath.second));

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
	/****************************可编程管线*******************************/
	VkPipelineShaderStageCreateInfo vertShaderStageCreateInfo{};
	vertShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageCreateInfo.module = m_mapSkyboxShaderModule.at(VK_SHADER_STAGE_VERTEX_BIT); //Bytecode
	vertShaderStageCreateInfo.pName = "main"; //要invoke的函数

	VkPipelineShaderStageCreateInfo fragShaderStageCreateInfo{};
	fragShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageCreateInfo.module = m_mapSkyboxShaderModule.at(VK_SHADER_STAGE_FRAGMENT_BIT); //Bytecode
	fragShaderStageCreateInfo.pName = "main"; //要invoke的函数

	VkPipelineShaderStageCreateInfo shaderStageCreateInfos[] = {
		vertShaderStageCreateInfo,
		fragShaderStageCreateInfo,
	};

	/*****************************固定管线*******************************/

	//-----------------------Dynamic State--------------------------//
	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo{};
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	std::vector<VkDynamicState> vecDynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};
	dynamicStateCreateInfo.dynamicStateCount = static_cast<UINT>(vecDynamicStates.size());
	dynamicStateCreateInfo.pDynamicStates = vecDynamicStates.data();

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

	//-----------------------Raserization State--------------------------//
	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo{};
	rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;	//开启后，超过远近平面的部分会被截断在远近平面上，而不是丢弃
	rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;	//开启后，禁止所有图元经过光栅化器
	rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;	//图元模式，可以是FILL、LINE、POINT
	rasterizationStateCreateInfo.lineWidth = 1.f;	//指定光栅化后的线段宽度
	rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_FRONT_BIT;	//剔除正向的可见性
	rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE; //顶点序，可以是顺时针cw或逆时针ccw
	rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE; //深度偏移，一般用于Shaodw Map中避免阴影痤疮
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
	depthStencilStateCreateInfo.depthTestEnable = VK_FALSE; //作为背景，始终在最远处，不进行深度检测
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
	pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
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
	
	//横向
	for (UINT i = 0; i < uiMeshGridSplitPointCount; ++i)
	{
		for (UINT j = 0; j < uiMeshGridSplitLineCount; ++j)
		{
			UINT uiStart = i * uiMeshGridSplitPointCount + j;
			m_vecMeshGridIndices.push_back(uiStart);
			m_vecMeshGridIndices.push_back(uiStart + 1);
		}
	}

	//纵向
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

void VulkanRenderer::RecreateMeshGridVertexBuffer()
{
	vkDeviceWaitIdle(m_LogicalDevice);

	vkDestroyBuffer(m_LogicalDevice, m_MeshGridVertexBuffer, nullptr);
	vkFreeMemory(m_LogicalDevice, m_MeshGridVertexBufferMemory, nullptr);

	CreateMeshGridVertexBuffer();
}

void VulkanRenderer::RecreateMeshGridIndexBuffer()
{
	vkDeviceWaitIdle(m_LogicalDevice);

	vkDestroyBuffer(m_LogicalDevice, m_MeshGridIndexBuffer, nullptr);
	vkFreeMemory(m_LogicalDevice, m_MeshGridIndexBufferMemory, nullptr);

	CreateMeshGridIndexBuffer();
}

void VulkanRenderer::CreateMeshGridVertexBuffer()
{
	CalcMeshGridVertexData();

	for (auto& vert : m_vecMeshGridVertices)
		vert.pos *= (m_fMeshGridSize / 2.f);

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
		auto shaderModule = DZW_VulkanUtils::CreateShaderModule(m_LogicalDevice, DZW_VulkanUtils::ReadShaderFile(spvPath.second));

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
	m_MeshGridUboData.model = glm::rotate(glm::mat4(1.f), glm::radians(90.f), { 1.f, 0.f, 0.f });
	m_MeshGridUboData.view = m_Camera.GetViewMatrix();
	m_MeshGridUboData.proj = m_Camera.GetProjMatrix();

	void* uniformBufferData;
	vkMapMemory(m_LogicalDevice, m_vecMeshGridUniformBufferMemories[uiIdx], 0, sizeof(MeshGridUniformBufferObject), 0, &uniformBufferData);
	memcpy(uniformBufferData, &m_MeshGridUboData, sizeof(MeshGridUniformBufferObject));
	vkUnmapMemory(m_LogicalDevice, m_vecMeshGridUniformBufferMemories[uiIdx]);
}

void VulkanRenderer::CreateMeshGridDescriptorSetLayout()
{
	//UniformBufferObject Binding
	VkDescriptorSetLayoutBinding uboLayoutBinding{};
	uboLayoutBinding.binding = 0; //对应Vertex Shader中的layout binding
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; //只需要在vertex stage生效
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
	/****************************可编程管线*******************************/
	VkPipelineShaderStageCreateInfo vertShaderStageCreateInfo{};
	vertShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageCreateInfo.module = m_mapMeshGridShaderModule.at(VK_SHADER_STAGE_VERTEX_BIT); //Bytecode
	vertShaderStageCreateInfo.pName = "main"; //要invoke的函数

	VkPipelineShaderStageCreateInfo fragShaderStageCreateInfo{};
	fragShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageCreateInfo.module = m_mapMeshGridShaderModule.at(VK_SHADER_STAGE_FRAGMENT_BIT); //Bytecode
	fragShaderStageCreateInfo.pName = "main"; //要invoke的函数

	VkPipelineShaderStageCreateInfo shaderStageCreateInfos[] = {
		vertShaderStageCreateInfo,
		fragShaderStageCreateInfo,
	};

	/*****************************固定管线*******************************/

	//-----------------------Dynamic State--------------------------//
	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo{};
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	std::vector<VkDynamicState> vecDynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
		VK_DYNAMIC_STATE_LINE_WIDTH,
	};
	dynamicStateCreateInfo.dynamicStateCount = static_cast<UINT>(vecDynamicStates.size());
	dynamicStateCreateInfo.pDynamicStates = vecDynamicStates.data();

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

	//-----------------------Raserization State--------------------------//
	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo{};
	rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;	//开启后，超过远近平面的部分会被截断在远近平面上，而不是丢弃
	rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;	//开启后，禁止所有图元经过光栅化器
	rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_LINE;	//图元模式，可以是FILL、LINE、POINT
	rasterizationStateCreateInfo.lineWidth = m_fMeshGridLineWidth;	//指定光栅化后的线段宽度
	rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE;
	rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; //顶点序，可以是顺时针cw或逆时针ccw
	rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE; //深度偏移，一般用于Shaodw Map中避免阴影痤疮
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
	depthStencilStateCreateInfo.depthTestEnable = VK_TRUE; //作为背景，始终在最远处，不进行深度检测
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
	pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
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

void VulkanRenderer::CreateEllipseVertexBuffer()
{
	ASSERT(m_Ellipse.m_vecVertices.size() > 0, "Vertex data empty");
	VkDeviceSize verticesSize = sizeof(m_Ellipse.m_vecVertices[0]) * m_Ellipse.m_vecVertices.size();

	CreateBufferAndBindMemory(verticesSize,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		m_EllipseVertexBuffer, m_EllipseVertexBufferMemory);
	TransferBufferDataByStageBuffer(m_Ellipse.m_vecVertices.data(), verticesSize, m_EllipseVertexBuffer);
}

void VulkanRenderer::CreateEllipseIndexBuffer()
{
	VkDeviceSize indicesSize = sizeof(m_Ellipse.m_vecIndices[0]) * m_Ellipse.m_vecIndices.size();
	if (indicesSize > 0)
		CreateBufferAndBindMemory(indicesSize,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			m_EllipseIndexBuffer, m_EllipseIndexBufferMemory);
	TransferBufferDataByStageBuffer(m_Ellipse.m_vecIndices.data(), indicesSize, m_EllipseIndexBuffer);
}

void VulkanRenderer::CreateEllipseUniformBuffers()
{
	m_vecEllipseUniformBuffers.resize(m_vecSwapChainImages.size());
	m_vecEllipseUniformBufferMemories.resize(m_vecSwapChainImages.size());

	for (size_t i = 0; i < m_vecSwapChainImages.size(); ++i)
	{
		CreateBufferAndBindMemory(sizeof(MeshGridUniformBufferObject),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			m_vecEllipseUniformBuffers[i],
			m_vecEllipseUniformBufferMemories[i]
		);
	}
}

void VulkanRenderer::UpdateEllipseUniformBuffer(UINT uiIdx)
{
	m_EllipseUboData.model = glm::rotate(glm::mat4(1.f), glm::radians(90.f), { 1.f, 0.f, 0.f }) * glm::scale(glm::mat4(1.f), {100.f, 100.f, 1.f});
	m_EllipseUboData.view = m_Camera.GetViewMatrix();
	m_EllipseUboData.proj = m_Camera.GetProjMatrix();

	void* uniformBufferData;
	vkMapMemory(m_LogicalDevice, m_vecEllipseUniformBufferMemories[uiIdx], 0, sizeof(MeshGridUniformBufferObject), 0, &uniformBufferData);
	memcpy(uniformBufferData, &m_EllipseUboData, sizeof(MeshGridUniformBufferObject));
	vkUnmapMemory(m_LogicalDevice, m_vecEllipseUniformBufferMemories[uiIdx]);
}

void VulkanRenderer::CreateEllipseDescriptorSetLayout()
{
	//UniformBufferObject Binding
	VkDescriptorSetLayoutBinding uboLayoutBinding{};
	uboLayoutBinding.binding = 0; //对应Vertex Shader中的layout binding
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; //只需要在vertex stage生效
	uboLayoutBinding.pImmutableSamplers = nullptr;

	std::vector<VkDescriptorSetLayoutBinding> vecDescriptorLayoutBinding = {
		uboLayoutBinding,
	};

	VkDescriptorSetLayoutCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	createInfo.bindingCount = static_cast<UINT>(vecDescriptorLayoutBinding.size());
	createInfo.pBindings = vecDescriptorLayoutBinding.data();

	VULKAN_ASSERT(vkCreateDescriptorSetLayout(m_LogicalDevice, &createInfo, nullptr, &m_EllipseDescriptorSetLayout), "Create ellipse descriptor layout failed");
}

void VulkanRenderer::CreateEllipseDescriptorPool()
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

	VULKAN_ASSERT(vkCreateDescriptorPool(m_LogicalDevice, &poolCreateInfo, nullptr, &m_EllipseDescriptorPool), "Create ellipse descriptor pool failed");
}

void VulkanRenderer::CreateEllipseDescriptorSets()
{
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorSetCount = static_cast<UINT>(m_vecSwapChainImages.size());
	allocInfo.descriptorPool = m_EllipseDescriptorPool;

	std::vector<VkDescriptorSetLayout> vecDupDescriptorSetLayout(m_vecSwapChainImages.size(), m_EllipseDescriptorSetLayout);
	allocInfo.pSetLayouts = vecDupDescriptorSetLayout.data();

	m_vecEllipseDescriptorSets.resize(m_vecSwapChainImages.size());
	VULKAN_ASSERT(vkAllocateDescriptorSets(m_LogicalDevice, &allocInfo, m_vecEllipseDescriptorSets.data()), "Allocate ellipse desctiprot sets failed");

	for (size_t i = 0; i < m_vecSwapChainImages.size(); ++i)
	{
		//ubo
		VkDescriptorBufferInfo descriptorBufferInfo{};
		descriptorBufferInfo.buffer = m_vecEllipseUniformBuffers[i];
		descriptorBufferInfo.offset = 0;
		descriptorBufferInfo.range = sizeof(MeshGridUniformBufferObject);

		VkWriteDescriptorSet uboWrite{};
		uboWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		uboWrite.dstSet = m_vecEllipseDescriptorSets[i];
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

void VulkanRenderer::CreateEllipseGraphicPipelineLayout()
{
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &m_EllipseDescriptorSetLayout;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

	VULKAN_ASSERT(vkCreatePipelineLayout(m_LogicalDevice, &pipelineLayoutCreateInfo, nullptr, &m_EllipseGraphicPipelineLayout), "Create ellipse pipeline layout failed");
}

void VulkanRenderer::CreateEllipseGraphicPipeline()
{
	/****************************可编程管线*******************************/
	VkPipelineShaderStageCreateInfo vertShaderStageCreateInfo{};
	vertShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageCreateInfo.module = m_mapMeshGridShaderModule.at(VK_SHADER_STAGE_VERTEX_BIT); //Bytecode
	vertShaderStageCreateInfo.pName = "main"; //要invoke的函数

	VkPipelineShaderStageCreateInfo fragShaderStageCreateInfo{};
	fragShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageCreateInfo.module = m_mapMeshGridShaderModule.at(VK_SHADER_STAGE_FRAGMENT_BIT); //Bytecode
	fragShaderStageCreateInfo.pName = "main"; //要invoke的函数

	VkPipelineShaderStageCreateInfo shaderStageCreateInfos[] = {
		vertShaderStageCreateInfo,
		fragShaderStageCreateInfo,
	};

	/*****************************固定管线*******************************/

	//-----------------------Dynamic State--------------------------//
	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo{};
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	std::vector<VkDynamicState> vecDynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
		VK_DYNAMIC_STATE_LINE_WIDTH,
	};
	dynamicStateCreateInfo.dynamicStateCount = static_cast<UINT>(vecDynamicStates.size());
	dynamicStateCreateInfo.pDynamicStates = vecDynamicStates.data();

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
	inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
	inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;


	//-----------------------Viewport State--------------------------//
	VkPipelineViewportStateCreateInfo viewportStateCreateInfo{};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.scissorCount = 1;

	//-----------------------Raserization State--------------------------//
	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo{};
	rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;	//开启后，超过远近平面的部分会被截断在远近平面上，而不是丢弃
	rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;	//开启后，禁止所有图元经过光栅化器
	rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_LINE;	//图元模式，可以是FILL、LINE、POINT
	rasterizationStateCreateInfo.lineWidth = m_fMeshGridLineWidth;	//指定光栅化后的线段宽度
	rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE;
	rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; //顶点序，可以是顺时针cw或逆时针ccw
	rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE; //深度偏移，一般用于Shaodw Map中避免阴影痤疮
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
	depthStencilStateCreateInfo.depthTestEnable = VK_TRUE; //作为背景，始终在最远处，不进行深度检测
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
	pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
	pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
	pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	pipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
	pipelineCreateInfo.pMultisampleState = &multisamplingStateCreateInfo;
	pipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
	pipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
	pipelineCreateInfo.layout = m_EllipseGraphicPipelineLayout;
	pipelineCreateInfo.renderPass = m_RenderPass;
	pipelineCreateInfo.subpass = 0;
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineCreateInfo.basePipelineIndex = -1;

	VULKAN_ASSERT(vkCreateGraphicsPipelines(m_LogicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &m_EllipseGraphicPipeline), "Create ellipse graphic pipeline failed");
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
	static auto startTime = std::chrono::high_resolution_clock::now();
	auto currentTime = std::chrono::high_resolution_clock::now();

	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	auto rotateComponent = glm::rotate(glm::mat4(1.f), glm::radians(m_fSkyboxRotateSpeed * time), { 0.f, 1.f, 0.f });

	m_SkyboxUboData.modelView = m_Camera.GetViewMatrix() * rotateComponent;
	m_SkyboxUboData.modelView[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f); //移除平移分量
	m_SkyboxUboData.proj = m_Camera.GetProjMatrix();

	void* uniformBufferData;
	vkMapMemory(m_LogicalDevice, m_vecSkyboxUniformBufferMemories[uiIdx], 0, sizeof(SkyboxUniformBufferObject), 0, &uniformBufferData);
	memcpy(uniformBufferData, &m_SkyboxUboData, sizeof(SkyboxUniformBufferObject));
	vkUnmapMemory(m_LogicalDevice, m_vecSkyboxUniformBufferMemories[uiIdx]);
}

void VulkanRenderer::CreateSkyboxDescriptorSetLayout()
{
	//UniformBufferObject Binding
	VkDescriptorSetLayoutBinding uboLayoutBinding{};
	uboLayoutBinding.binding = 0; //对应Vertex Shader中的layout binding
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; //只需要在vertex stage生效
	uboLayoutBinding.pImmutableSamplers = nullptr;

	//Cubemap ImageSampler Binding
	VkDescriptorSetLayoutBinding CubemapSamplerLayoutBinding{};
	CubemapSamplerLayoutBinding.binding = 1; ////对应Fragment Shader中的layout binding
	CubemapSamplerLayoutBinding.descriptorCount = 1;
	CubemapSamplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	CubemapSamplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; //只用于fragment stage
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
		imageInfo.imageView = m_SkyboxTexture->m_ImageView;
		imageInfo.sampler = m_SkyboxTexture->m_Sampler;

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
	//在Record之前更新UBO

	//UpdateUniformBuffer(m_uiCurFrameIdx);

	UpdateCommonMVPUniformBuffer(m_uiCurFrameIdx);

	if (m_bEnableSkybox)
		UpdateSkyboxUniformBuffer(m_uiCurFrameIdx);

	//if (m_bEnableMeshGrid)
	//	UpdateMeshGridUniformBuffer(m_uiCurFrameIdx);

	//if (m_bEnableEllipse)
	//	UpdateEllipseUniformBuffer(m_uiCurFrameIdx);

	//if (m_bEnableBlinnPhong)
	//{
	//	UpdateBlinnPhongMVPUniformBuffer(m_uiCurFrameIdx);
	//	UpdateBlinnPhongLightUniformBuffer(m_uiCurFrameIdx);
	//	UpdateBlinnPhongMaterialUniformBuffer(m_uiCurFrameIdx);
	//}

	//if (m_bEnablePBR)
	//{
	//	UpdatePBRMVPUniformBuffer(m_uiCurFrameIdx);
	//	UpdatePBRLightUniformBuffer(m_uiCurFrameIdx);
	//	UpdatePBRMaterialUniformBuffer(m_uiCurFrameIdx);
	//}


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

	//vkCmdSetViewport，vkCmdSetScissor类似函数应用于commandBuffer后续所有的绘制命令
	//应该在最开始绑定渲染管线之前设置
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
	
	if (m_bEnableSkybox)
	{
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_SkyboxGraphicPipeline);
		VkBuffer skyboxVertexBuffers[] = {
			m_SkyboxModel->m_VertexBuffer,
		};
		VkDeviceSize skyboxOffsets[]{ 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, skyboxVertexBuffers, skyboxOffsets);
		vkCmdBindIndexBuffer(commandBuffer, m_SkyboxModel->m_IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdBindDescriptorSets(commandBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			m_SkyboxGraphicPipelineLayout,
			0, 1,
			&m_vecSkyboxDescriptorSets[uiIdx],
			0, NULL);

		vkCmdDrawIndexed(commandBuffer, static_cast<UINT>(m_SkyboxModel->m_vecIndices.size()), 1, 0, 0, 0);
	}

	if (m_bEnableMeshGrid)
	{
		if (m_fLastMeshGridSplit != m_fMeshGridSplit)
		{
			RecreateMeshGridVertexBuffer();
			RecreateMeshGridIndexBuffer();
			m_fLastMeshGridSplit = m_fMeshGridSplit;
		}
		if (m_fLastMeshGridSize != m_fMeshGridSize)
		{
			RecreateMeshGridVertexBuffer();
			m_fLastMeshGridSize = m_fMeshGridSize;
		}
		vkCmdSetLineWidth(commandBuffer, m_fMeshGridLineWidth);
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
		vkCmdSetLineWidth(commandBuffer, 1.f);
	}

	if (m_bEnableEllipse)
	{
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_EllipseGraphicPipeline);
		VkBuffer ellipseVertexBuffers[] = {
			m_EllipseVertexBuffer,
		};
		VkDeviceSize ellipseOffsets[]{ 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, ellipseVertexBuffers, ellipseOffsets);
		vkCmdBindIndexBuffer(commandBuffer, m_EllipseIndexBuffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdBindDescriptorSets(commandBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			m_EllipseGraphicPipelineLayout,
			0, 1,
			&m_vecEllipseDescriptorSets[uiIdx],
			0, NULL);

		vkCmdDrawIndexed(commandBuffer, static_cast<UINT>(m_Ellipse.m_vecIndices.size()), 1, 0, 0, 0);
	}

	//if (m_bEnableBlinnPhong)
	//{
	//	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_BlinnPhongGraphicPipeline);
	//	VkBuffer blinnPhongVertexBuffers[] = {
	//		m_BlinnPhongModel.m_VertexBuffer,
	//	};
	//	VkDeviceSize blinnPhongOffsets[]{ 0 };
	//	vkCmdBindVertexBuffers(commandBuffer, 0, 1, blinnPhongVertexBuffers, blinnPhongOffsets);
	//	vkCmdBindIndexBuffer(commandBuffer, m_BlinnPhongModel.m_IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
	//	vkCmdBindDescriptorSets(commandBuffer,
	//		VK_PIPELINE_BIND_POINT_GRAPHICS,
	//		m_BlinnPhongGraphicPipelineLayout,
	//		0, 1,
	//		&m_vecBlinnPhongDescriptorSets[uiIdx],
	//		0, NULL);

	//	vkCmdDrawIndexed(commandBuffer, static_cast<UINT>(m_BlinnPhongModel.m_vecIndices.size()), 1, 0, 0, 0);
	//}

	//if (m_bEnablePBR)
	//{
	//	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PBRGraphicPipeline);
	//	VkBuffer PBRVertexBuffers[] = {
	//		m_PBRModel.m_VertexBuffer,
	//	};
	//	VkDeviceSize PBROffsets[]{ 0 };
	//	vkCmdBindVertexBuffers(commandBuffer, 0, 1, PBRVertexBuffers, PBROffsets);
	//	vkCmdBindIndexBuffer(commandBuffer, m_PBRModel.m_IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
	//	vkCmdBindDescriptorSets(commandBuffer,
	//		VK_PIPELINE_BIND_POINT_GRAPHICS,
	//		m_PBRGraphicPipelineLayout,
	//		0, 1,
	//		&m_vecPBRDescriptorSets[uiIdx],
	//		0, NULL);

	//	vkCmdDrawIndexed(commandBuffer, static_cast<UINT>(m_PBRModel.m_vecIndices.size()), 1, 0, 0, 0);
	//}

	//vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicPipeline);
	//VkBuffer vertexBuffers[] = {
	//	m_Model.m_VertexBuffer,
	//};
	//VkDeviceSize offsets[]{ 0 };
	//vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

	//vkCmdBindIndexBuffer(commandBuffer, m_Model.m_IndexBuffer, 0, VK_INDEX_TYPE_UINT32);

	//for (UINT i = 0; i < INSTANCE_NUM; ++i)
	//{
	//	UINT uiDynamicOffset = i * static_cast<UINT>(m_DynamicAlignment);

	//	vkCmdBindDescriptorSets(commandBuffer,
	//		VK_PIPELINE_BIND_POINT_GRAPHICS, //descriptorSet并非Pipeline独有，因此需要指定是用于Graphic Pipeline还是Compute Pipeline
	//		m_GraphicPipelineLayout, //PipelineLayout中指定了descriptorSetLayout
	//		0,	//descriptorSet数组中第一个元素的下标 
	//		1,	//descriptorSet数组中元素的个数
	//		&m_vecDescriptorSets[uiIdx],
	//		1, //启用动态Uniform偏移
	//		&uiDynamicOffset	//指定动态Uniform的偏移
	//	);

	//	vkCmdDrawIndexed(commandBuffer, static_cast<UINT>(m_Model.m_vecIndices.size()), 1, 0, 0, 0);
	//}

	m_testObjModel->Draw(commandBuffer, m_CommonGraphicPipeline, m_CommonGraphicPipelineLayout);

	//m_testGLTFModel->Draw(commandBuffer, m_GLTFGraphicPipeline, m_GLTFGraphicPipelineLayout);

	g_UI.Render(uiIdx);

	vkCmdEndRenderPass(commandBuffer);

	VULKAN_ASSERT(vkEndCommandBuffer(commandBuffer), "End command buffer failed");
}

void VulkanRenderer::UpdateUniformBuffer(UINT uiIdx)
{
	static auto startTime = std::chrono::high_resolution_clock::now();
	auto currentTime = std::chrono::high_resolution_clock::now();

	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();


	m_UboData.view = m_Camera.GetViewMatrix();
	m_UboData.proj = m_Camera.GetProjMatrix();

	void* uniformBufferData;
	vkMapMemory(m_LogicalDevice, m_vecUniformBufferMemories[uiIdx], 0, m_UboBufferSize, 0, &uniformBufferData);
	memcpy(uniformBufferData, &m_UboData, m_UboBufferSize);
	vkUnmapMemory(m_LogicalDevice, m_vecUniformBufferMemories[uiIdx]);

	glm::mat4* pModelMat = nullptr;
	float* pTextureIdx = nullptr;
	for (UINT i = 0; i < INSTANCE_NUM; ++i)
	{
		ASSERT(i < m_vecPlanetInfo.size());
		auto planetInfo = m_vecPlanetInfo[i];

		pModelMat = (glm::mat4*)((size_t)m_DynamicUboData.model + (i * m_DynamicAlignment));

		//auto translateComponent = glm::translate(glm::mat4(1.f), { planetInfo.fOrbitRadius * 100.f, 0.f, 0.f });
		//auto rotateComponent = glm::rotate(glm::mat4(1.f), glm::radians(planetInfo.fRotationAxisDegree), { 0.f, 0.f, -1.f });
		//auto scaleComponent = glm::scale(glm::mat4(1.f), glm::vec3(static_cast<float>(planetInfo.fDiameter / 100.f)));

		//*pModelMat = translateComponent * rotateComponent * scaleComponent;
		*pModelMat = glm::translate(glm::mat4(1.f), { (float)i * 10.f, 0.f, 0.f});
		
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
	if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) && !ImGui::IsAnyItemActive())
		m_Camera.Tick();

	//等待fence的值变为signaled
	vkWaitForFences(m_LogicalDevice, 1, &m_vecInFlightFences[m_uiCurFrameIdx], VK_TRUE, UINT64_MAX);

	if (m_bNeedResize)
	{
		WindowResize();
		m_bNeedResize = false;
	}

	g_UI.StartNewFrame();

	uint32_t uiImageIdx;
	VkResult res = vkAcquireNextImageKHR(m_LogicalDevice, m_SwapChain, UINT64_MAX,
		m_vecImageAvailableSemaphores[m_uiCurFrameIdx], VK_NULL_HANDLE, &uiImageIdx);
	if (res != VK_SUCCESS)
	{
		if (res == VK_ERROR_OUT_OF_DATE_KHR)
		{
			//SwapChain与WindowSurface不兼容，无法继续渲染
			//一般发生在window尺寸改变时
			m_bNeedResize = true;
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

	vkResetCommandBuffer(m_vecCommandBuffers[m_uiCurFrameIdx], 0);

	RecordCommandBuffer(m_vecCommandBuffers[m_uiCurFrameIdx], m_uiCurFrameIdx);

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
			m_bNeedResize = true;
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

void VulkanRenderer::WindowResize()
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

	m_Camera.SetViewportSize(static_cast<float>(nWidth), static_cast<float>(nHeight));

	//需要重建的资源：
	//1. 和窗口大小相关的：Depth(Image, Memory, ImageView)，SwapChain Image，Viewport/Scissors
	//2. 引用了DepthImageView的：FrameBuffer
	//3. 引用了SwapChain Image的：FrameBuffer
	//4. 引用了FrameBuffer的：CommandBuffer(RenderPassBeginInfo)
	//5. 引用了ViewportScissors的：Pipeline（如果viewport/scissor非dynamic）
	//6. 如果颜色格式或色彩空间发生了变化：RenderPass
	//7. 如果SwapChain中的ImageCount发生了变化：相关的各种同步对象，fence，semaphore

	vkDeviceWaitIdle(m_LogicalDevice);

	//swap cain
	CreateSwapChain(); //在创建中销毁旧的SwapChain

	//depth
	vkDestroyImageView(m_LogicalDevice, m_DepthImageView, nullptr);
	vkDestroyImage(m_LogicalDevice, m_DepthImage, nullptr);
	vkFreeMemory(m_LogicalDevice, m_DepthImageMemory, nullptr);
	CreateDepthImage();
	CreateDepthImageView();

	//image views
	for (const auto& imageView : m_vecSwapChainImageViews)
	{
		vkDestroyImageView(m_LogicalDevice, imageView, nullptr);
	}
	CreateSwapChainImages();
	CreateSwapChainImageViews();

	//frame buffers
	for (const auto& frameBuffer : m_vecSwapChainFrameBuffers)
	{
		vkDestroyFramebuffer(m_LogicalDevice, frameBuffer, nullptr);
	}
	CreateSwapChainFrameBuffers();

	//UI
	g_UI.Resize();

	//command buffers
	vkFreeCommandBuffers(m_LogicalDevice, m_CommandPool,
		static_cast<UINT>(m_vecCommandBuffers.size()),
		m_vecCommandBuffers.data());
	
	CreateCommandBuffers();
}

void VulkanRenderer::LoadPlanetInfo()
{
	std::filesystem::path configPath = "./config.json";
	ASSERT(std::filesystem::exists(configPath), std::format("planet config {} not exist", configPath.string()));
	std::ifstream file(configPath);

	nlohmann::json jsonFile;
	jsonFile << file;

	m_vecPlanetInfo.clear();

	std::vector<std::string> vecPlanet = {"Sun", "Mercury", "Venus", "Earth", "Mars", "Juipter", "Saturn", "Uranus", "Neptune"};

	for (auto it = vecPlanet.begin(); it != vecPlanet.end(); ++it)
	{
		PlanetInfo info;
		auto node = jsonFile[*it];
		info.strDesc = node["Desc"];
		info.fDiameter = static_cast<float>(node["Diameter"]);
		info.fRotationAxisDegree = node["RotationAxisDegree"];
		info.fRotationPeriod = node["RotationPeriod"];
		info.fRevolutionPeriod = node["RevolutionPeriod"];
		info.fOrbitRadius = node["OrbitRadius"];

		m_vecPlanetInfo.push_back(info);
	}
}

void VulkanRenderer::InitBlinnPhongLightMaterialInfo()
{
	m_BlinnPhongPointLight.position = { 5.f, 0.f, 0.f };
	m_BlinnPhongPointLight.fIntensify = 1.f;

	//黄金
	m_BlinnPhongMaterial.ambientCoefficient = { 0.24725, 0.1995, 0.0745, 1.0 };
	m_BlinnPhongMaterial.diffuseCoefficient = { 0.75164, 0.60648, 0.22648, 1.0 };
	m_BlinnPhongMaterial.specularCoefficient = { 0.62828, 0.5558, 0.36607, 1.0 };
	m_BlinnPhongMaterial.fShininess = 51.2;
}

void VulkanRenderer::CreateBlinnPhongShaderModule()
{
	std::unordered_map<VkShaderStageFlagBits, std::filesystem::path> mapBlinnPhongShaderPath = {
	{ VK_SHADER_STAGE_VERTEX_BIT,	"./Assert/Shader/BlinnPhong/vert.spv" },
	{ VK_SHADER_STAGE_FRAGMENT_BIT,	"./Assert/Shader/BlinnPhong/frag.spv" },
	};
	ASSERT(mapBlinnPhongShaderPath.size() > 0, "Detect no shader spv file");

	m_mapBlinnPhongShaderModule.clear();

	for (const auto& spvPath : mapBlinnPhongShaderPath)
	{
		auto shaderModule = DZW_VulkanUtils::CreateShaderModule(m_LogicalDevice, DZW_VulkanUtils::ReadShaderFile(spvPath.second));

		m_mapBlinnPhongShaderModule[spvPath.first] = shaderModule;
	}
}

void VulkanRenderer::CreateBlinnPhongMVPUniformBuffers()
{
	m_vecBlinnPhongMVPUniformBuffers.resize(m_vecSwapChainImages.size());
	m_vecBlinnPhongMVPUniformBufferMemories.resize(m_vecSwapChainImages.size());

	for (size_t i = 0; i < m_vecSwapChainImages.size(); ++i)
	{
		CreateBufferAndBindMemory(sizeof(BlinnPhongMVPUniformBufferObject),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			m_vecBlinnPhongMVPUniformBuffers[i],
			m_vecBlinnPhongMVPUniformBufferMemories[i]
		);
	}
}

void VulkanRenderer::UpdateBlinnPhongMVPUniformBuffer(UINT uiIdx)
{
	m_BlinnPhongMVPUBOData.model = glm::translate(glm::mat4(1.f), {0.f, 0.f, 0.f});
	m_BlinnPhongMVPUBOData.view = m_Camera.GetViewMatrix();
	m_BlinnPhongMVPUBOData.proj = m_Camera.GetProjMatrix();

	m_BlinnPhongMVPUBOData.mv_normal = glm::transpose(glm::inverse(m_BlinnPhongMVPUBOData.view * m_BlinnPhongMVPUBOData.model));

	void* uniformBufferData;
	vkMapMemory(m_LogicalDevice, m_vecBlinnPhongMVPUniformBufferMemories[uiIdx], 0, sizeof(BlinnPhongMVPUniformBufferObject), 0, &uniformBufferData);
	memcpy(uniformBufferData, &m_BlinnPhongMVPUBOData, sizeof(BlinnPhongMVPUniformBufferObject));
	vkUnmapMemory(m_LogicalDevice, m_vecBlinnPhongMVPUniformBufferMemories[uiIdx]);
}

void VulkanRenderer::CreateBlinnPhongLightUniformBuffers()
{
	m_vecBlinnPhongLightUniformBuffers.resize(m_vecSwapChainImages.size());
	m_vecBlinnPhongLightUniformBufferMemories.resize(m_vecSwapChainImages.size());

	for (size_t i = 0; i < m_vecSwapChainImages.size(); ++i)
	{
		CreateBufferAndBindMemory(sizeof(BlinnPhongLightUniformBufferObject),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			m_vecBlinnPhongLightUniformBuffers[i],
			m_vecBlinnPhongLightUniformBufferMemories[i]
		);
	}
}

void VulkanRenderer::UpdateBlinnPhongLightUniformBuffer(UINT uiIdx)
{
	m_BlinnPhongLightUBOData.position = m_Camera.GetViewMatrix() * glm::vec4(m_BlinnPhongPointLight.position, 1.0); //转到视图空间

	m_BlinnPhongLightUBOData.color = m_BlinnPhongPointLight.color;

	m_BlinnPhongLightUBOData.intensify = m_BlinnPhongPointLight.fIntensify;

	m_BlinnPhongLightUBOData.constant = m_BlinnPhongPointLight.fConstantAttenuation;
	m_BlinnPhongLightUBOData.linear = m_BlinnPhongPointLight.fLinearAttenuation;
	m_BlinnPhongLightUBOData.quadratic = m_BlinnPhongPointLight.fQuadraticAttenuation;

	void* uniformBufferData;
	vkMapMemory(m_LogicalDevice, m_vecBlinnPhongLightUniformBufferMemories[uiIdx], 0, sizeof(BlinnPhongLightUniformBufferObject), 0, &uniformBufferData);
	memcpy(uniformBufferData, &m_BlinnPhongLightUBOData, sizeof(BlinnPhongLightUniformBufferObject));
	vkUnmapMemory(m_LogicalDevice, m_vecBlinnPhongLightUniformBufferMemories[uiIdx]);
}

void VulkanRenderer::CreateBlinnPhongMaterialUniformBuffers()
{
	m_vecBlinnPhongMaterialUniformBuffers.resize(m_vecSwapChainImages.size());
	m_vecBlinnPhongMaterialUniformBufferMemories.resize(m_vecSwapChainImages.size());

	for (size_t i = 0; i < m_vecSwapChainImages.size(); ++i)
	{
		CreateBufferAndBindMemory(sizeof(BlinnPhongMaterialUniformBufferObject),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			m_vecBlinnPhongMaterialUniformBuffers[i],
			m_vecBlinnPhongMaterialUniformBufferMemories[i]
		);
	}
}

void VulkanRenderer::UpdateBlinnPhongMaterialUniformBuffer(UINT uiIdx)
{
	m_BlinnPhongMaterialUBOData.ambient = m_BlinnPhongMaterial.ambientCoefficient;
	m_BlinnPhongMaterialUBOData.diffuse = m_BlinnPhongMaterial.ambientCoefficient;
	m_BlinnPhongMaterialUBOData.specular = m_BlinnPhongMaterial.specularCoefficient;
	m_BlinnPhongMaterialUBOData.shininess = m_BlinnPhongMaterial.fShininess;

	void* uniformBufferData;
	vkMapMemory(m_LogicalDevice, m_vecBlinnPhongMaterialUniformBufferMemories[uiIdx], 0, sizeof(BlinnPhongMaterialUniformBufferObject), 0, &uniformBufferData);
	memcpy(uniformBufferData, &m_BlinnPhongMaterialUBOData, sizeof(BlinnPhongMaterialUniformBufferObject));
	vkUnmapMemory(m_LogicalDevice, m_vecBlinnPhongMaterialUniformBufferMemories[uiIdx]);
}

void VulkanRenderer::CreateBlinnPhongDescriptorSetLayout()
{
	//MVP UBO Binding
	VkDescriptorSetLayoutBinding MVPUBOLayoutBinding{};
	MVPUBOLayoutBinding.binding = 0;
	MVPUBOLayoutBinding.descriptorCount = 1;
	MVPUBOLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	MVPUBOLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	MVPUBOLayoutBinding.pImmutableSamplers = nullptr;

	//Light UBO Binding
	VkDescriptorSetLayoutBinding LightUBOLayoutBinding{};
	LightUBOLayoutBinding.binding = 1;
	LightUBOLayoutBinding.descriptorCount = 1;
	LightUBOLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	LightUBOLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	LightUBOLayoutBinding.pImmutableSamplers = nullptr;

	//Material UBO Binding
	VkDescriptorSetLayoutBinding MaterialUBOLayoutBinding{};
	MaterialUBOLayoutBinding.binding = 2;
	MaterialUBOLayoutBinding.descriptorCount = 1;
	MaterialUBOLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	MaterialUBOLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	MaterialUBOLayoutBinding.pImmutableSamplers = nullptr;

	std::vector<VkDescriptorSetLayoutBinding> vecDescriptorLayoutBinding = {
		MVPUBOLayoutBinding,
		LightUBOLayoutBinding,
		MaterialUBOLayoutBinding
	};

	VkDescriptorSetLayoutCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	createInfo.bindingCount = static_cast<UINT>(vecDescriptorLayoutBinding.size());
	createInfo.pBindings = vecDescriptorLayoutBinding.data();

	VULKAN_ASSERT(vkCreateDescriptorSetLayout(m_LogicalDevice, &createInfo, nullptr, &m_BlinnPhongDescriptorSetLayout), "Create BlinnPhong descriptor layout failed");
}

void VulkanRenderer::CreateBlinnPhongDescriptorPool()
{
	VkDescriptorPoolSize MVPUBOPoolSize{};
	MVPUBOPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	MVPUBOPoolSize.descriptorCount = static_cast<UINT>(m_vecSwapChainImages.size());

	VkDescriptorPoolSize lightUBOPoolSize{};
	lightUBOPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	lightUBOPoolSize.descriptorCount = static_cast<UINT>(m_vecSwapChainImages.size());

	VkDescriptorPoolSize materialUBOPoolSize{};
	materialUBOPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	materialUBOPoolSize.descriptorCount = static_cast<UINT>(m_vecSwapChainImages.size());

	std::vector<VkDescriptorPoolSize> vecPoolSize = {
		MVPUBOPoolSize,
		lightUBOPoolSize,
		materialUBOPoolSize
	};

	VkDescriptorPoolCreateInfo poolCreateInfo{};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCreateInfo.poolSizeCount = static_cast<UINT>(vecPoolSize.size());
	poolCreateInfo.pPoolSizes = vecPoolSize.data();
	poolCreateInfo.maxSets = static_cast<UINT>(m_vecSwapChainImages.size());

	VULKAN_ASSERT(vkCreateDescriptorPool(m_LogicalDevice, &poolCreateInfo, nullptr, &m_BlinnPhongDescriptorPool), "Create BlinnPhong descriptor pool failed");
}

void VulkanRenderer::CreateBlinnPhongDescriptorSets()
{
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorSetCount = static_cast<UINT>(m_vecSwapChainImages.size());
	allocInfo.descriptorPool = m_BlinnPhongDescriptorPool;

	std::vector<VkDescriptorSetLayout> vecDupDescriptorSetLayout(m_vecSwapChainImages.size(), m_BlinnPhongDescriptorSetLayout);
	allocInfo.pSetLayouts = vecDupDescriptorSetLayout.data();

	m_vecBlinnPhongDescriptorSets.resize(m_vecSwapChainImages.size());
	VULKAN_ASSERT(vkAllocateDescriptorSets(m_LogicalDevice, &allocInfo, m_vecBlinnPhongDescriptorSets.data()), "Allocate BlinnPhong desctiprot sets failed");

	for (size_t i = 0; i < m_vecSwapChainImages.size(); ++i)
	{
		VkDescriptorBufferInfo MVPDescriptorBufferInfo{};
		MVPDescriptorBufferInfo.buffer = m_vecBlinnPhongMVPUniformBuffers[i];
		MVPDescriptorBufferInfo.offset = 0;
		MVPDescriptorBufferInfo.range = sizeof(BlinnPhongMVPUniformBufferObject);

		VkDescriptorBufferInfo lightDscriptorBufferInfo{};
		lightDscriptorBufferInfo.buffer = m_vecBlinnPhongLightUniformBuffers[i];
		lightDscriptorBufferInfo.offset = 0;
		lightDscriptorBufferInfo.range = sizeof(BlinnPhongLightUniformBufferObject);

		VkDescriptorBufferInfo materialDescriptorBufferInfo{};
		materialDescriptorBufferInfo.buffer = m_vecBlinnPhongMaterialUniformBuffers[i];
		materialDescriptorBufferInfo.offset = 0;
		materialDescriptorBufferInfo.range = sizeof(BlinnPhongMaterialUniformBufferObject);

		VkWriteDescriptorSet MVPUBOWrite{};
		MVPUBOWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		MVPUBOWrite.dstSet = m_vecBlinnPhongDescriptorSets[i];
		MVPUBOWrite.dstBinding = 0;
		MVPUBOWrite.dstArrayElement = 0;
		MVPUBOWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		MVPUBOWrite.descriptorCount = 1;
		MVPUBOWrite.pBufferInfo = &MVPDescriptorBufferInfo;

		VkWriteDescriptorSet lightUBOWrite{};
		lightUBOWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		lightUBOWrite.dstSet = m_vecBlinnPhongDescriptorSets[i];
		lightUBOWrite.dstBinding = 1;
		lightUBOWrite.dstArrayElement = 0;
		lightUBOWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		lightUBOWrite.descriptorCount = 1;
		lightUBOWrite.pBufferInfo = &lightDscriptorBufferInfo;

		VkWriteDescriptorSet materialUBOWrite{};
		materialUBOWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		materialUBOWrite.dstSet = m_vecBlinnPhongDescriptorSets[i];
		materialUBOWrite.dstBinding = 2;
		materialUBOWrite.dstArrayElement = 0;
		materialUBOWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		materialUBOWrite.descriptorCount = 1;
		materialUBOWrite.pBufferInfo = &materialDescriptorBufferInfo;

		std::vector<VkWriteDescriptorSet> vecDescriptorWrite = {
			MVPUBOWrite,
			lightUBOWrite,
			materialUBOWrite
		};

		vkUpdateDescriptorSets(m_LogicalDevice, static_cast<UINT>(vecDescriptorWrite.size()), vecDescriptorWrite.data(), 0, nullptr);
	}
}

void VulkanRenderer::CreateBlinnPhongGraphicPipelineLayout()
{
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &m_BlinnPhongDescriptorSetLayout;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

	VULKAN_ASSERT(vkCreatePipelineLayout(m_LogicalDevice, &pipelineLayoutCreateInfo, nullptr, &m_BlinnPhongGraphicPipelineLayout), "Create BlinnPhong pipeline layout failed");
}

void VulkanRenderer::CreateBlinnPhongGraphicPipeline()
{
	/****************************可编程管线*******************************/
	VkPipelineShaderStageCreateInfo vertShaderStageCreateInfo{};
	vertShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageCreateInfo.module = m_mapBlinnPhongShaderModule.at(VK_SHADER_STAGE_VERTEX_BIT); //Bytecode
	vertShaderStageCreateInfo.pName = "main"; //要invoke的函数

	VkPipelineShaderStageCreateInfo fragShaderStageCreateInfo{};
	fragShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageCreateInfo.module = m_mapBlinnPhongShaderModule.at(VK_SHADER_STAGE_FRAGMENT_BIT); //Bytecode
	fragShaderStageCreateInfo.pName = "main"; //要invoke的函数

	VkPipelineShaderStageCreateInfo shaderStageCreateInfos[] = {
		vertShaderStageCreateInfo,
		fragShaderStageCreateInfo,
	};

	/*****************************固定管线*******************************/

	//-----------------------Dynamic State--------------------------//
	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo{};
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	std::vector<VkDynamicState> vecDynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};
	dynamicStateCreateInfo.dynamicStateCount = static_cast<UINT>(vecDynamicStates.size());
	dynamicStateCreateInfo.pDynamicStates = vecDynamicStates.data();

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

	//-----------------------Raserization State--------------------------//
	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo{};
	rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;	//开启后，超过远近平面的部分会被截断在远近平面上，而不是丢弃
	rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;	//开启后，禁止所有图元经过光栅化器
	rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;	//图元模式，可以是FILL、LINE、POINT
	rasterizationStateCreateInfo.lineWidth = 1.f;	//指定光栅化后的线段宽度
	rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE;
	rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; //顶点序，可以是顺时针cw或逆时针ccw
	rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE; //深度偏移，一般用于Shaodw Map中避免阴影痤疮
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
	depthStencilStateCreateInfo.depthTestEnable = VK_TRUE; //作为背景，始终在最远处，不进行深度检测
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
	pipelineCreateInfo.stageCount = static_cast<UINT>(m_mapBlinnPhongShaderModule.size());
	pipelineCreateInfo.pStages = shaderStageCreateInfos;
	pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
	pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
	pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	pipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
	pipelineCreateInfo.pMultisampleState = &multisamplingStateCreateInfo;
	pipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
	pipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
	pipelineCreateInfo.layout = m_BlinnPhongGraphicPipelineLayout;
	pipelineCreateInfo.renderPass = m_RenderPass;
	pipelineCreateInfo.subpass = 0;
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineCreateInfo.basePipelineIndex = -1;

	VULKAN_ASSERT(vkCreateGraphicsPipelines(m_LogicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &m_BlinnPhongGraphicPipeline), "Create BlinnPhong graphic pipeline failed");
}

void VulkanRenderer::InitPBRLightMaterialInfo()
{
	m_PBRPointLight.position = { 5.f, 0.f, 0.f };
	m_PBRPointLight.fIntensify = 1.f;

	m_PBRMaterial.baseColor = { 1.0, 0.782, 0.344 };
	m_PBRMaterial.fMetallic = 1.0f;
	m_PBRMaterial.fRoughness = 0.f;
	m_PBRMaterial.fAO = 0.f;
}

void VulkanRenderer::CreatePBRShaderModule()
{
	std::unordered_map<VkShaderStageFlagBits, std::filesystem::path> mapPBRShaderPath = {
	{ VK_SHADER_STAGE_VERTEX_BIT,	"./Assert/Shader/PBR/vert.spv" },
	{ VK_SHADER_STAGE_FRAGMENT_BIT,	"./Assert/Shader/PBR/frag.spv" },
	};
	ASSERT(mapPBRShaderPath.size() > 0, "Detect no shader spv file");

	m_mapPBRShaderModule.clear();

	for (const auto& spvPath : mapPBRShaderPath)
	{
		auto shaderModule = DZW_VulkanUtils::CreateShaderModule(m_LogicalDevice, DZW_VulkanUtils::ReadShaderFile(spvPath.second));

		m_mapPBRShaderModule[spvPath.first] = shaderModule;
	}
}

void VulkanRenderer::CreatePBRMVPUniformBuffers()
{
	m_vecPBRMVPUniformBuffers.resize(m_vecSwapChainImages.size());
	m_vecPBRMVPUniformBufferMemories.resize(m_vecSwapChainImages.size());

	for (size_t i = 0; i < m_vecSwapChainImages.size(); ++i)
	{
		CreateBufferAndBindMemory(sizeof(PBRMVPUniformBufferObject),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			m_vecPBRMVPUniformBuffers[i],
			m_vecPBRMVPUniformBufferMemories[i]
		);
	}
}

void VulkanRenderer::UpdatePBRMVPUniformBuffer(UINT uiIdx)
{
	m_PBRMVPUBOData.model = glm::translate(glm::mat4(1.f), { 10.f, 0.f, 0.f });
	m_PBRMVPUBOData.view = m_Camera.GetViewMatrix();
	m_PBRMVPUBOData.proj = m_Camera.GetProjMatrix();

	m_PBRMVPUBOData.mv_normal = glm::transpose(glm::inverse(m_PBRMVPUBOData.view * m_PBRMVPUBOData.model));

	void* uniformBufferData;
	vkMapMemory(m_LogicalDevice, m_vecPBRMVPUniformBufferMemories[uiIdx], 0, sizeof(PBRMVPUniformBufferObject), 0, &uniformBufferData);
	memcpy(uniformBufferData, &m_PBRMVPUBOData, sizeof(PBRMVPUniformBufferObject));
	vkUnmapMemory(m_LogicalDevice, m_vecPBRMVPUniformBufferMemories[uiIdx]);
}

void VulkanRenderer::CreatePBRLightUniformBuffers()
{
	m_vecPBRLightUniformBuffers.resize(m_vecSwapChainImages.size());
	m_vecPBRLightUniformBufferMemories.resize(m_vecSwapChainImages.size());

	for (size_t i = 0; i < m_vecSwapChainImages.size(); ++i)
	{
		CreateBufferAndBindMemory(sizeof(PBRLightUniformBufferObject),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			m_vecPBRLightUniformBuffers[i],
			m_vecPBRLightUniformBufferMemories[i]
		);
	}
}

void VulkanRenderer::UpdatePBRLightUniformBuffer(UINT uiIdx)
{
	m_PBRLightUBOData.position = m_Camera.GetViewMatrix() * glm::vec4(m_PBRPointLight.position, 1.0); //转到视图空间

	m_PBRLightUBOData.color = m_PBRPointLight.color;

	m_PBRLightUBOData.intensify = m_PBRPointLight.fIntensify;

	m_PBRLightUBOData.constant = m_PBRPointLight.fConstantAttenuation;
	m_PBRLightUBOData.linear = m_PBRPointLight.fLinearAttenuation;
	m_PBRLightUBOData.quadratic = m_PBRPointLight.fQuadraticAttenuation;

	void* uniformBufferData;
	vkMapMemory(m_LogicalDevice, m_vecPBRLightUniformBufferMemories[uiIdx], 0, sizeof(PBRLightUniformBufferObject), 0, &uniformBufferData);
	memcpy(uniformBufferData, &m_PBRLightUBOData, sizeof(PBRLightUniformBufferObject));
	vkUnmapMemory(m_LogicalDevice, m_vecPBRLightUniformBufferMemories[uiIdx]);
}

void VulkanRenderer::CreatePBRMaterialUniformBuffers()
{
	m_vecPBRMaterialUniformBuffers.resize(m_vecSwapChainImages.size());
	m_vecPBRMaterialUniformBufferMemories.resize(m_vecSwapChainImages.size());

	for (size_t i = 0; i < m_vecSwapChainImages.size(); ++i)
	{
		CreateBufferAndBindMemory(sizeof(PBRMaterialUniformBufferObject),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			m_vecPBRMaterialUniformBuffers[i],
			m_vecPBRMaterialUniformBufferMemories[i]
		);
	}
}

void VulkanRenderer::UpdatePBRMaterialUniformBuffer(UINT uiIdx)
{
	m_PBRMaterialUBOData.baseColor = m_PBRMaterial.baseColor;
	m_PBRMaterialUBOData.metallic = m_PBRMaterial.fMetallic;
	m_PBRMaterialUBOData.roughness = m_PBRMaterial.fRoughness;
	m_PBRMaterialUBOData.ao = m_PBRMaterial.fAO;

	void* uniformBufferData;
	vkMapMemory(m_LogicalDevice, m_vecPBRMaterialUniformBufferMemories[uiIdx], 0, sizeof(PBRMaterialUniformBufferObject), 0, &uniformBufferData);
	memcpy(uniformBufferData, &m_PBRMaterialUBOData, sizeof(PBRMaterialUniformBufferObject));
	vkUnmapMemory(m_LogicalDevice, m_vecPBRMaterialUniformBufferMemories[uiIdx]);
}

void VulkanRenderer::CreatePBRDescriptorSetLayout()
{
	//MVP UBO Binding
	VkDescriptorSetLayoutBinding MVPUBOLayoutBinding{};
	MVPUBOLayoutBinding.binding = 0;
	MVPUBOLayoutBinding.descriptorCount = 1;
	MVPUBOLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	MVPUBOLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	MVPUBOLayoutBinding.pImmutableSamplers = nullptr;

	//Light UBO Binding
	VkDescriptorSetLayoutBinding LightUBOLayoutBinding{};
	LightUBOLayoutBinding.binding = 1;
	LightUBOLayoutBinding.descriptorCount = 1;
	LightUBOLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	LightUBOLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	LightUBOLayoutBinding.pImmutableSamplers = nullptr;

	//Material UBO Binding
	VkDescriptorSetLayoutBinding MaterialUBOLayoutBinding{};
	MaterialUBOLayoutBinding.binding = 2;
	MaterialUBOLayoutBinding.descriptorCount = 1;
	MaterialUBOLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	MaterialUBOLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	MaterialUBOLayoutBinding.pImmutableSamplers = nullptr;

	std::vector<VkDescriptorSetLayoutBinding> vecDescriptorLayoutBinding = {
		MVPUBOLayoutBinding,
		LightUBOLayoutBinding,
		MaterialUBOLayoutBinding
	};

	VkDescriptorSetLayoutCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	createInfo.bindingCount = static_cast<UINT>(vecDescriptorLayoutBinding.size());
	createInfo.pBindings = vecDescriptorLayoutBinding.data();

	VULKAN_ASSERT(vkCreateDescriptorSetLayout(m_LogicalDevice, &createInfo, nullptr, &m_PBRDescriptorSetLayout), "Create PBR descriptor layout failed");
}

void VulkanRenderer::CreatePBRDescriptorPool()
{
	VkDescriptorPoolSize MVPUBOPoolSize{};
	MVPUBOPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	MVPUBOPoolSize.descriptorCount = static_cast<UINT>(m_vecSwapChainImages.size());

	VkDescriptorPoolSize lightUBOPoolSize{};
	lightUBOPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	lightUBOPoolSize.descriptorCount = static_cast<UINT>(m_vecSwapChainImages.size());

	VkDescriptorPoolSize materialUBOPoolSize{};
	materialUBOPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	materialUBOPoolSize.descriptorCount = static_cast<UINT>(m_vecSwapChainImages.size());

	std::vector<VkDescriptorPoolSize> vecPoolSize = {
		MVPUBOPoolSize,
		lightUBOPoolSize,
		materialUBOPoolSize
	};

	VkDescriptorPoolCreateInfo poolCreateInfo{};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCreateInfo.poolSizeCount = static_cast<UINT>(vecPoolSize.size());
	poolCreateInfo.pPoolSizes = vecPoolSize.data();
	poolCreateInfo.maxSets = static_cast<UINT>(m_vecSwapChainImages.size());

	VULKAN_ASSERT(vkCreateDescriptorPool(m_LogicalDevice, &poolCreateInfo, nullptr, &m_PBRDescriptorPool), "Create PBR descriptor pool failed");
}

void VulkanRenderer::CreatePBRDescriptorSets()
{
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorSetCount = static_cast<UINT>(m_vecSwapChainImages.size());
	allocInfo.descriptorPool = m_PBRDescriptorPool;

	std::vector<VkDescriptorSetLayout> vecDupDescriptorSetLayout(m_vecSwapChainImages.size(), m_PBRDescriptorSetLayout);
	allocInfo.pSetLayouts = vecDupDescriptorSetLayout.data();

	m_vecPBRDescriptorSets.resize(m_vecSwapChainImages.size());
	VULKAN_ASSERT(vkAllocateDescriptorSets(m_LogicalDevice, &allocInfo, m_vecPBRDescriptorSets.data()), "Allocate PBR desctiprot sets failed");

	for (size_t i = 0; i < m_vecSwapChainImages.size(); ++i)
	{
		VkDescriptorBufferInfo MVPDescriptorBufferInfo{};
		MVPDescriptorBufferInfo.buffer = m_vecPBRMVPUniformBuffers[i];
		MVPDescriptorBufferInfo.offset = 0;
		MVPDescriptorBufferInfo.range = sizeof(PBRMVPUniformBufferObject);

		VkDescriptorBufferInfo lightDscriptorBufferInfo{};
		lightDscriptorBufferInfo.buffer = m_vecPBRLightUniformBuffers[i];
		lightDscriptorBufferInfo.offset = 0;
		lightDscriptorBufferInfo.range = sizeof(PBRLightUniformBufferObject);

		VkDescriptorBufferInfo materialDescriptorBufferInfo{};
		materialDescriptorBufferInfo.buffer = m_vecPBRMaterialUniformBuffers[i];
		materialDescriptorBufferInfo.offset = 0;
		materialDescriptorBufferInfo.range = sizeof(PBRMaterialUniformBufferObject);

		VkWriteDescriptorSet MVPUBOWrite{};
		MVPUBOWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		MVPUBOWrite.dstSet = m_vecPBRDescriptorSets[i];
		MVPUBOWrite.dstBinding = 0;
		MVPUBOWrite.dstArrayElement = 0;
		MVPUBOWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		MVPUBOWrite.descriptorCount = 1;
		MVPUBOWrite.pBufferInfo = &MVPDescriptorBufferInfo;

		VkWriteDescriptorSet lightUBOWrite{};
		lightUBOWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		lightUBOWrite.dstSet = m_vecPBRDescriptorSets[i];
		lightUBOWrite.dstBinding = 1;
		lightUBOWrite.dstArrayElement = 0;
		lightUBOWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		lightUBOWrite.descriptorCount = 1;
		lightUBOWrite.pBufferInfo = &lightDscriptorBufferInfo;

		VkWriteDescriptorSet materialUBOWrite{};
		materialUBOWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		materialUBOWrite.dstSet = m_vecPBRDescriptorSets[i];
		materialUBOWrite.dstBinding = 2;
		materialUBOWrite.dstArrayElement = 0;
		materialUBOWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		materialUBOWrite.descriptorCount = 1;
		materialUBOWrite.pBufferInfo = &materialDescriptorBufferInfo;

		std::vector<VkWriteDescriptorSet> vecDescriptorWrite = {
			MVPUBOWrite,
			lightUBOWrite,
			materialUBOWrite
		};

		vkUpdateDescriptorSets(m_LogicalDevice, static_cast<UINT>(vecDescriptorWrite.size()), vecDescriptorWrite.data(), 0, nullptr);
	}
}

void VulkanRenderer::CreatePBRGraphicPipelineLayout()
{
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &m_PBRDescriptorSetLayout;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

	VULKAN_ASSERT(vkCreatePipelineLayout(m_LogicalDevice, &pipelineLayoutCreateInfo, nullptr, &m_PBRGraphicPipelineLayout), "Create PBR pipeline layout failed");
}

void VulkanRenderer::CreatePBRGraphicPipeline()
{
	/****************************可编程管线*******************************/
	VkPipelineShaderStageCreateInfo vertShaderStageCreateInfo{};
	vertShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageCreateInfo.module = m_mapPBRShaderModule.at(VK_SHADER_STAGE_VERTEX_BIT); //Bytecode
	vertShaderStageCreateInfo.pName = "main"; //要invoke的函数

	VkPipelineShaderStageCreateInfo fragShaderStageCreateInfo{};
	fragShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageCreateInfo.module = m_mapPBRShaderModule.at(VK_SHADER_STAGE_FRAGMENT_BIT); //Bytecode
	fragShaderStageCreateInfo.pName = "main"; //要invoke的函数

	VkPipelineShaderStageCreateInfo shaderStageCreateInfos[] = {
		vertShaderStageCreateInfo,
		fragShaderStageCreateInfo,
	};

	/*****************************固定管线*******************************/

	//-----------------------Dynamic State--------------------------//
	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo{};
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	std::vector<VkDynamicState> vecDynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};
	dynamicStateCreateInfo.dynamicStateCount = static_cast<UINT>(vecDynamicStates.size());
	dynamicStateCreateInfo.pDynamicStates = vecDynamicStates.data();

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

	//-----------------------Raserization State--------------------------//
	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo{};
	rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;	//开启后，超过远近平面的部分会被截断在远近平面上，而不是丢弃
	rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;	//开启后，禁止所有图元经过光栅化器
	rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;	//图元模式，可以是FILL、LINE、POINT
	rasterizationStateCreateInfo.lineWidth = 1.f;	//指定光栅化后的线段宽度
	rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE;
	rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; //顶点序，可以是顺时针cw或逆时针ccw
	rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE; //深度偏移，一般用于Shaodw Map中避免阴影痤疮
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
	depthStencilStateCreateInfo.depthTestEnable = VK_TRUE; //作为背景，始终在最远处，不进行深度检测
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
	pipelineCreateInfo.stageCount = static_cast<UINT>(m_mapPBRShaderModule.size());
	pipelineCreateInfo.pStages = shaderStageCreateInfos;
	pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
	pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
	pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	pipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
	pipelineCreateInfo.pMultisampleState = &multisamplingStateCreateInfo;
	pipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
	pipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
	pipelineCreateInfo.layout = m_PBRGraphicPipelineLayout;
	pipelineCreateInfo.renderPass = m_RenderPass;
	pipelineCreateInfo.subpass = 0;
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineCreateInfo.basePipelineIndex = -1;

	VULKAN_ASSERT(vkCreateGraphicsPipelines(m_LogicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &m_PBRGraphicPipeline), "Create PBR graphic pipeline failed");
}

void VulkanRenderer::CreateCommonShader()
{
	std::unordered_map<VkShaderStageFlagBits, std::filesystem::path> mapShaderPath = {
		{ VK_SHADER_STAGE_VERTEX_BIT,	"./Assert/Shader/Common/vert.spv" },
		{ VK_SHADER_STAGE_FRAGMENT_BIT,	"./Assert/Shader/Common/frag.spv" },
	};
	ASSERT(mapShaderPath.size() > 0, "Detect no shader spv file");

	m_mapCommonShaderModule.clear();

	for (const auto& spvPath : mapShaderPath)
	{
		auto shaderModule = DZW_VulkanUtils::CreateShaderModule(m_LogicalDevice, DZW_VulkanUtils::ReadShaderFile(spvPath.second));

		m_mapCommonShaderModule[spvPath.first] = shaderModule;
	}
}

void VulkanRenderer::CreateCommonMVPUniformBufferAndMemory()
{
	CreateBufferAndBindMemory(sizeof(CommonMVPUniformBufferObject),
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		m_CommonMVPUniformBuffer,
		m_CommonMVPUniformBufferMemory
	);
}

void VulkanRenderer::UpdateCommonMVPUniformBuffer(UINT uiIdx)
{
	m_CommonMVPUboData.model = glm::translate(glm::mat4(1.f), { 0.f, 0.f, 0.f });
	m_CommonMVPUboData.view = m_Camera.GetViewMatrix();
	m_CommonMVPUboData.proj = m_Camera.GetProjMatrix();

	void* uniformBufferData;
	vkMapMemory(m_LogicalDevice, m_CommonMVPUniformBufferMemory, 0, sizeof(CommonMVPUniformBufferObject), 0, &uniformBufferData);
	memcpy(uniformBufferData, &m_CommonMVPUboData, sizeof(CommonMVPUniformBufferObject));
	vkUnmapMemory(m_LogicalDevice, m_CommonMVPUniformBufferMemory);
}

void VulkanRenderer::CreateCommonDescriptorSetLayout()
{
	//MVP UBO Binding
	VkDescriptorSetLayoutBinding uboLayoutBinding{};
	uboLayoutBinding.binding = 0; //对应Vertex Shader中的layout binding
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; //只需要在vertex stage生效
	uboLayoutBinding.pImmutableSamplers = nullptr;

	std::vector<VkDescriptorSetLayoutBinding> vecDescriptorLayoutBinding = {
		uboLayoutBinding,
	};

	VkDescriptorSetLayoutCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	createInfo.bindingCount = static_cast<UINT>(vecDescriptorLayoutBinding.size());
	createInfo.pBindings = vecDescriptorLayoutBinding.data();

	VULKAN_ASSERT(vkCreateDescriptorSetLayout(m_LogicalDevice, &createInfo, nullptr, &m_CommonDescriptorSetLayout), "Create common descriptor layout failed");
}

void VulkanRenderer::CreateCommonDescriptorPool()
{
	//MVP UBO
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

	VULKAN_ASSERT(vkCreateDescriptorPool(m_LogicalDevice, &poolCreateInfo, nullptr, &m_CommonDescriptorPool), "Create common descriptor pool failed");
}

void VulkanRenderer::CreateCommonGraphicPipelineLayout()
{
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &m_CommonDescriptorSetLayout;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

	VULKAN_ASSERT(vkCreatePipelineLayout(m_LogicalDevice, &pipelineLayoutCreateInfo, nullptr, &m_CommonGraphicPipelineLayout), "Create common pipeline layout failed");
}

void VulkanRenderer::CreateCommonGraphicPipeline()
{
	/****************************可编程管线*******************************/

	ASSERT(m_mapCommonShaderModule.find(VK_SHADER_STAGE_VERTEX_BIT) != m_mapCommonShaderModule.end(), "No vertex shader module");
	ASSERT(m_mapCommonShaderModule.find(VK_SHADER_STAGE_FRAGMENT_BIT) != m_mapCommonShaderModule.end(), "No fragment shader module");

	VkPipelineShaderStageCreateInfo vertShaderStageCreateInfo{};
	vertShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageCreateInfo.module = m_mapCommonShaderModule.at(VK_SHADER_STAGE_VERTEX_BIT); //Bytecode
	vertShaderStageCreateInfo.pName = "main"; //要invoke的函数

	VkPipelineShaderStageCreateInfo fragShaderStageCreateInfo{};
	fragShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageCreateInfo.module = m_mapCommonShaderModule.at(VK_SHADER_STAGE_FRAGMENT_BIT); //Bytecode
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

	std::vector<VkDynamicState> vecDynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};
	dynamicStateCreateInfo.dynamicStateCount = static_cast<UINT>(vecDynamicStates.size());
	dynamicStateCreateInfo.pDynamicStates = vecDynamicStates.data();


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

	//-----------------------Raserization State--------------------------//
	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo{};
	rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;	//开启后，超过远近平面的部分会被截断在远近平面上，而不是丢弃
	rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;	//开启后，禁止所有图元经过光栅化器
	rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;	//图元模式，可以是FILL、LINE、POINT
	rasterizationStateCreateInfo.lineWidth = 1.f;	//指定光栅化后的线段宽度
	rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE;	//剔除模式，可以是NONE、FRONT、BACK、FRONT_AND_BACK
	rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE; //顶点序，可以是顺时针cw或逆时针ccw
	rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE; //深度偏移，一般用于Shaodw Map中避免阴影痤疮
	rasterizationStateCreateInfo.depthBiasConstantFactor = 0.f;
	rasterizationStateCreateInfo.depthBiasClamp = 0.f;
	rasterizationStateCreateInfo.depthBiasSlopeFactor = 0.f;

	//-----------------------Multisample State--------------------------//
	VkPipelineMultisampleStateCreateInfo multisamplingStateCreateInfo{};
	multisamplingStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisamplingStateCreateInfo.sampleShadingEnable = false;
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
	pipelineCreateInfo.stageCount = static_cast<UINT>(m_mapCommonShaderModule.size());
	pipelineCreateInfo.pStages = shaderStageCreateInfos;
	pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
	pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
	pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	pipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
	pipelineCreateInfo.pMultisampleState = &multisamplingStateCreateInfo;
	pipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
	pipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
	pipelineCreateInfo.layout = m_CommonGraphicPipelineLayout;
	pipelineCreateInfo.renderPass = m_RenderPass;
	pipelineCreateInfo.subpass = 0;
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineCreateInfo.basePipelineIndex = -1;

	//创建管线时，如果提供了VkPipelineCache对象，Vulkan会尝试从中重用数据
	//如果没有可重用的数据，新的数据会被添加到缓存中
	VkPipelineCache pipelineCache = m_CommonGraphicPipelineCache;

	VULKAN_ASSERT(vkCreateGraphicsPipelines(m_LogicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &m_CommonGraphicPipeline), "Create common graphic pipeline failed");
}

void VulkanRenderer::CreateGLTFShader()
{
	std::unordered_map<VkShaderStageFlagBits, std::filesystem::path> mapShaderPath = {
	{ VK_SHADER_STAGE_VERTEX_BIT,	"./Assert/Shader/glTF/vert.spv" },
	{ VK_SHADER_STAGE_FRAGMENT_BIT,	"./Assert/Shader/glTF/frag.spv" },
	};
	ASSERT(mapShaderPath.size() > 0, "Detect no shader spv file");

	m_mapGLTFShaderModule.clear();

	for (const auto& spvPath : mapShaderPath)
	{
		auto shaderModule = DZW_VulkanUtils::CreateShaderModule(m_LogicalDevice, DZW_VulkanUtils::ReadShaderFile(spvPath.second));

		m_mapGLTFShaderModule[spvPath.first] = shaderModule;
	}
}

void VulkanRenderer::CreateGLTFDescriptorSetLayout()
{
	//baseColor sampler binding
	VkDescriptorSetLayoutBinding baseColorSamplerLayoutBinding{};
	baseColorSamplerLayoutBinding.binding = 0;
	baseColorSamplerLayoutBinding.descriptorCount = 1;
	baseColorSamplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	baseColorSamplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; //只用于fragment stage
	baseColorSamplerLayoutBinding.pImmutableSamplers = nullptr;

	//normal sampler binding
	VkDescriptorSetLayoutBinding normalSamplerLayoutBinding{};
	normalSamplerLayoutBinding.binding = 1;
	normalSamplerLayoutBinding.descriptorCount = 1;
	normalSamplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	normalSamplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; //只用于fragment stage
	normalSamplerLayoutBinding.pImmutableSamplers = nullptr;

	//occlusion+metallic+roughness sampler binding
	VkDescriptorSetLayoutBinding omrSamplerLayoutBinding{};
	omrSamplerLayoutBinding.binding = 2;
	omrSamplerLayoutBinding.descriptorCount = 1;
	omrSamplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	omrSamplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; //只用于fragment stage
	omrSamplerLayoutBinding.pImmutableSamplers = nullptr;

	std::vector<VkDescriptorSetLayoutBinding> vecDescriptorLayoutBinding = {
		baseColorSamplerLayoutBinding,
		normalSamplerLayoutBinding,
		omrSamplerLayoutBinding,
	};

	VkDescriptorSetLayoutCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	createInfo.bindingCount = static_cast<UINT>(vecDescriptorLayoutBinding.size());
	createInfo.pBindings = vecDescriptorLayoutBinding.data();

	VULKAN_ASSERT(vkCreateDescriptorSetLayout(m_LogicalDevice, &createInfo, nullptr, &m_GLTFDescriptorSetLayout), "Create gltf descriptor layout failed");
}

void VulkanRenderer::CreateGLTFDescriptorPool()
{
	//baseColor sampler
	VkDescriptorPoolSize baseColorSamplerPoolSize{};
	baseColorSamplerPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	baseColorSamplerPoolSize.descriptorCount = static_cast<UINT>(m_vecSwapChainImages.size()) * 10;

	//normal sampler
	VkDescriptorPoolSize normalSamplerPoolSize{};
	normalSamplerPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	normalSamplerPoolSize.descriptorCount = static_cast<UINT>(m_vecSwapChainImages.size()) * 10;

	//occlusion+metallic+roughness sampler
	VkDescriptorPoolSize omrSamplerPoolSize{};
	omrSamplerPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	omrSamplerPoolSize.descriptorCount = static_cast<UINT>(m_vecSwapChainImages.size()) * 10;

	std::vector<VkDescriptorPoolSize> vecPoolSize = {
		baseColorSamplerPoolSize,
		normalSamplerPoolSize,
		omrSamplerPoolSize
	};

	VkDescriptorPoolCreateInfo poolCreateInfo{};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCreateInfo.poolSizeCount = static_cast<UINT>(vecPoolSize.size());
	poolCreateInfo.pPoolSizes = vecPoolSize.data();
	poolCreateInfo.maxSets = 100; //可以设置得很大，但并不意味着可以无限制地分配描述符集，并且描述符池也可能会占用大量的内存
	//todo：应该创建swapChain中的Image个，然后update图片

	VULKAN_ASSERT(vkCreateDescriptorPool(m_LogicalDevice, &poolCreateInfo, nullptr, &m_GLTFDescriptorPool), "Create gltf descriptor pool failed");
}

void VulkanRenderer::CreateGLTFGraphicPipelineLayout()
{
	VkPushConstantRange MVPPushConstantRange = {};
	MVPPushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; //在顶点着色器中使用
	MVPPushConstantRange.offset = 0;
	MVPPushConstantRange.size = sizeof(glm::mat4) * 3; //model/view/proj三个mat4

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &m_GLTFDescriptorSetLayout;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	pipelineLayoutCreateInfo.pPushConstantRanges = &MVPPushConstantRange;

	VULKAN_ASSERT(vkCreatePipelineLayout(m_LogicalDevice, &pipelineLayoutCreateInfo, nullptr, &m_GLTFGraphicPipelineLayout), "Create gltf pipeline layout failed");
}

void VulkanRenderer::CreateGLTFGraphicPipeline()
{
	/****************************可编程管线*******************************/

	ASSERT(m_mapGLTFShaderModule.find(VK_SHADER_STAGE_VERTEX_BIT) != m_mapGLTFShaderModule.end(), "No vertex shader module");
	ASSERT(m_mapGLTFShaderModule.find(VK_SHADER_STAGE_FRAGMENT_BIT) != m_mapGLTFShaderModule.end(), "No fragment shader module");

	VkPipelineShaderStageCreateInfo vertShaderStageCreateInfo{};
	vertShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageCreateInfo.module = m_mapGLTFShaderModule.at(VK_SHADER_STAGE_VERTEX_BIT); //Bytecode
	vertShaderStageCreateInfo.pName = "main"; //要invoke的函数

	VkPipelineShaderStageCreateInfo fragShaderStageCreateInfo{};
	fragShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageCreateInfo.module = m_mapGLTFShaderModule.at(VK_SHADER_STAGE_FRAGMENT_BIT); //Bytecode
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

	std::vector<VkDynamicState> vecDynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};
	dynamicStateCreateInfo.dynamicStateCount = static_cast<UINT>(vecDynamicStates.size());
	dynamicStateCreateInfo.pDynamicStates = vecDynamicStates.data();


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
	multisamplingStateCreateInfo.sampleShadingEnable = false;
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
	pipelineCreateInfo.stageCount = static_cast<UINT>(m_mapGLTFShaderModule.size());
	pipelineCreateInfo.pStages = shaderStageCreateInfos;
	pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
	pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
	pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	pipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
	pipelineCreateInfo.pMultisampleState = &multisamplingStateCreateInfo;
	pipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
	pipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
	pipelineCreateInfo.layout = m_GLTFGraphicPipelineLayout;
	pipelineCreateInfo.renderPass = m_RenderPass;
	pipelineCreateInfo.subpass = 0;
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineCreateInfo.basePipelineIndex = -1;

	//创建管线时，如果提供了VkPipelineCache对象，Vulkan会尝试从中重用数据
	//如果没有可重用的数据，新的数据会被添加到缓存中
	VkPipelineCache pipelineCache = m_CommonGraphicPipelineCache;

	VULKAN_ASSERT(vkCreateGraphicsPipelines(m_LogicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &m_GLTFGraphicPipeline), "Create gltf graphic pipeline failed");
}
