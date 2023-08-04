#pragma once


#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include "glm/glm.hpp"

#include "Core.h"
#include "Camera.h"

#include "VulkanWrap.h"

struct PlanetInfo
{
	std::string strDesc;
	float fDiameter = 0.f;	//单位：km
	float fRotationAxisDegree = 0.f; //单位：度
	float fRotationPeriod = 0.f; //单位：天
	float fRevolutionPeriod = 0.f; //单位：年
	float fOrbitRadius = 0.f; //单位：AU，即149.6百万千米
};

namespace std
{
	template<> struct hash<Vertex3D>
	{
		size_t operator()(Vertex3D const& vertex) const
		{
			size_t h1 = hash<glm::vec3>()(vertex.pos);
			size_t h2 = hash<glm::vec3>()(vertex.color);
			size_t h3 = hash<glm::vec2>()(vertex.texCoord);
			size_t h4 = hash<glm::vec3>()(vertex.normal);
			return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
		}
	};
}

struct SwapChainSupportInfo
{
	VkSurfaceCapabilitiesKHR capabilities;	//SwapChain容量相关信息
	std::vector<VkSurfaceFormatKHR> vecSurfaceFormats;	//支持哪些图像格式
	std::vector<VkPresentModeKHR> vecPresentModes;	//支持哪些表现模式，如二缓、三缓等
};

struct PhysicalDeviceInfo
{
	PhysicalDeviceInfo()
	{
		nRateScore = 0;
		graphicFamilyIdx = std::nullopt;
		presentFamilyIdx = std::nullopt;
	}

	VkPhysicalDeviceProperties properties;
	VkPhysicalDeviceFeatures features;
	std::vector<VkQueueFamilyProperties> vecQueueFamilies;

	std::vector<VkExtensionProperties> vecAvaliableDeviceExtensions;

	int nRateScore;

	std::string strDeviceTypeName;

	std::optional<UINT> graphicFamilyIdx;
	std::optional<UINT> presentFamilyIdx;

	bool HaveGraphicAndPresentQueueFamily()
	{
		return graphicFamilyIdx.has_value() && presentFamilyIdx.has_value();
	}

	bool IsGraphicAndPresentQueueFamilySame()
	{
		return HaveGraphicAndPresentQueueFamily() && (graphicFamilyIdx == presentFamilyIdx);
	}

	SwapChainSupportInfo swapChainSupportInfo;

	VkPhysicalDeviceMemoryProperties memoryProperties;
};



class VulkanRenderer
{
public:
	VulkanRenderer();
	~VulkanRenderer();

	VulkanRenderer& operator=(const VulkanRenderer& other) { return *this; }

	void Init();
	void Loop();
	void Clean();

private:
	static void FrameBufferResizeCallBack(GLFWwindow* pWindow, int nWidth, int nHeight);
	static void MouseButtonCallBack(GLFWwindow* pWindow, int nButton, int nAction, int nMods);
	void InitWindow();

	void QueryGLFWExtensions();
	void QueryValidationLayerExtensions();
	void QueryAllValidExtensions();
	bool IsExtensionValid(const std::string& strExtensionName);
	bool CheckChosedExtensionValid();

	void QueryValidationLayers();
	void QueryAllValidValidationLayers();
	bool IsValidationLayerValid(const std::string& strValidationLayerName);
	bool CheckChosedValidationLayerValid();

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallBack(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageServerity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData);
	VkResult CreateDebugUtilsMessengerEXT(
		VkInstance instance,
		const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
		const VkAllocationCallbacks* pAllocator,
		VkDebugUtilsMessengerEXT* pDebugMessenger);
	void DestoryDebugUtilsMessengerEXT(
		VkInstance instance,
		VkDebugUtilsMessengerEXT debugMessenger,
		const VkAllocationCallbacks* pAllocator);
	void FillDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& messengerCreateInfo);
	void SetupDebugMessenger(const VkDebugUtilsMessengerCreateInfoEXT& messengerCreateInfo);

	void CreateInstance();

	void CreateWindowSurface();

	void QueryAllValidPhysicalDevice();
	int RatePhysicalDevice(PhysicalDeviceInfo& deviceInfo);
	void PickBestPhysicalDevice();

	bool checkDeviceExtensionSupport(const PhysicalDeviceInfo& deviceInfo);
	void CreateLogicalDevice();


	VkSurfaceFormatKHR ChooseSwapChainSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& vecAvailableFormats);
	VkPresentModeKHR ChooseSwapChainPresentMode(const std::vector<VkPresentModeKHR>& vecAvailableModes);
	VkExtent2D ChooseSwapChainSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
	UINT ChooseSwapChainImageCount(const VkSurfaceCapabilitiesKHR& capabilities);
	void CreateSwapChain();

	VkSurfaceFormatKHR ChooseUISwapChainSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& vecAvailableFormats);

	VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, UINT uiMipLevelCount, UINT uiLayerCount, UINT uiFaceCount);

	void CreateDepthImage();
	void CreateDepthImageView();

	void CreateSwapChainImages();
	void CreateSwapChainImageViews();
	void CreateSwapChainFrameBuffers();

	VkFormat ChooseDepthFormat(bool bCheckSamplingSupport);
	void CreateRenderPass();

	void CreateTransferCommandPool();
	VkCommandBuffer BeginSingleTimeCommand();
	void EndSingleTimeCommand(VkCommandBuffer commandBuffer);

	void CreateShader();

	struct UniformBufferObject
	{
		glm::mat4 view;
		glm::mat4 proj;
		float lod = 0.f;
	};

	struct DynamicUniformBufferObject
	{
		glm::mat4* model;
		float* fTextureIndex;
	};

	void AllocateBufferMemory(VkMemoryPropertyFlags propertyFlags, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
	void CreateBufferAndBindMemory(VkDeviceSize deviceSize, VkBufferUsageFlags usageFlags,
		VkMemoryPropertyFlags propertyFlags, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
	void CreateUniformBuffers();

	void AllocateImageMemory(VkMemoryPropertyFlags propertyFlags, VkImage& image, VkDeviceMemory& bufferMemory);
	void CreateImageAndBindMemory(UINT uiWidth, UINT uiHeight, UINT uiMipLevelCount, UINT uiLayerCount, UINT uiFaceCount,
		VkSampleCountFlagBits sampleCount, VkFormat format,
		VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
		VkImage& image, VkDeviceMemory& imageMemory);
	bool CheckFormatHasStencilComponent(VkFormat format);
	void ChangeImageLayout(VkImage image, VkFormat format, UINT uiMipLevelCount, UINT uiLayerCount, UINT uiFaceCount, VkImageLayout oldLayout, VkImageLayout newLayout);
	void TransferImageDataByStageBuffer(void* pData, VkDeviceSize imageSize, VkImage& image, UINT uiWidth, UINT uiHeight, DZW_VulkanWrap::Texture& texture, ktxTexture* pKtxTexture);
	//void CreateTextureImageAndFillData();
	//void CreateTextureImageView();

	void CreateDescriptorSetLayout();
	void CreateDescriptorPool();
	void CreateDescriptorSets();


	void TransferBufferDataByStageBuffer(void* pData, VkDeviceSize imageSize, VkBuffer& buffer);

	void CreateCommandPool();
	void CreateCommandBuffers();

	void CreateGraphicPipelineLayout();
	void CreateGraphicPipeline();

	void CreateSyncObjects();


	void RecordCommandBuffer(VkCommandBuffer& commandBuffer, UINT uiIdx);
	void UpdateUniformBuffer(UINT uiIdx);
	void Render();

	void WindowResize();

private:
	void LoadTexture(const std::filesystem::path& filepath, DZW_VulkanWrap::Texture& texture);
	void FreeTexture(DZW_VulkanWrap::Texture& texture);

	void LoadOBJ(DZW_VulkanWrap::Model& model);
	void LoadGLTF(DZW_VulkanWrap::Model& model);
	void LoadModel(const std::filesystem::path& filepath, DZW_VulkanWrap::Model& model);
	void FreeModel(DZW_VulkanWrap::Model& model);

public:
	std::vector<PlanetInfo> m_vecPlanetInfo;
	void LoadPlanetInfo();

public:
	Camera m_Camera;
	void SetupCamera();

	UINT m_uiFPS;
	UINT m_uiFrameCounter;
	UINT GetFPS() { return m_uiFPS; }

public:
	GLFWwindow* GetWindow() { return m_pWindow; }
	VkInstance& GetInstance() { return m_Instance; }
	VkRenderPass& GetRenderPass() { return m_RenderPass; }
	VkPhysicalDevice& GetPhysicalDevice() { return m_PhysicalDevice; }
	VkDevice& GetLogicalDevice() { return m_LogicalDevice; }
	UINT GetGraphicQueueIdx() { return m_mapPhysicalDeviceInfo.at(m_PhysicalDevice).graphicFamilyIdx.value(); }
	VkQueue& GetGraphicQueue() { return m_GraphicQueue; }
	VkDescriptorPool& GetDescriptorPool() { return m_DescriptorPool; }
	UINT GetSwapChainMinImageCount() { return m_uiSwapChainMinImageCount; }
	UINT GetSwapChainImageCount() { return static_cast<UINT>(m_vecSwapChainImages.size()); }

	VkExtent2D& GetSwapChainExtent2D() { return m_SwapChainExtent2D; }

	VkImageView& GetSwapChainImageView(UINT uiIdx) { return m_vecSwapChainImageViews[uiIdx]; }

	VkCommandPool& GetTransferCommandPool() { return m_TransferCommandPool; }
	VkCommandBuffer BeginSingleTimeCommandBuffer() { return BeginSingleTimeCommand(); }
	void EndSingleTimeCommandBuffer(VkCommandBuffer commandBuffer) { return EndSingleTimeCommand(commandBuffer); }

	VkFormat GetSwapChainFormat() { return m_SwapChainFormat; }

	VkPipeline& GetPipeline() { return m_GraphicPipeline; }

	PhysicalDeviceInfo& GetPhysicalDeviceInfo() { return m_mapPhysicalDeviceInfo.at(m_PhysicalDevice); }

	UINT FindSuitableMemoryTypeIndex(UINT typeFilter, VkMemoryPropertyFlags properties);


	void SetTextureLod(float fLod) { m_UboData.lod = fLod; }
	UINT GetTextureMaxLod() { return m_Texture.m_uiMipLevelNum; }

	VkCommandBuffer& GetCommandBuffer(UINT uiIdx) { return m_vecCommandBuffers[uiIdx]; }

	glm::vec3 GetCameraPosition() { return m_Camera.GetPosition(); }

	bool* GetSkyboxEnable() { return &m_bEnableSkybox; }
	float* GetSkyboxRotateSpeed() { return &m_fSkyboxRotateSpeed; }

	bool* GetMeshGridEnable() { return &m_bEnableMeshGrid; }
	float* GetMeshGridSize() { return &m_fMeshGridSize; }
	float* GetMeshGridSplit() { return &m_fMeshGridSplit; }
	float* GetMeshGridLineWidth() { return &m_fMeshGridLineWidth; }

	DZW_LightWrap::BlinnPhongPointLight* GetBlinnPhongPointLight() { return &m_BlinnPhongPointLight; }
	DZW_MaterialWrap::BlinnPhongMaterial* GetBlinnPhongMaterial() { return &m_BlinnPhongMaterial; }
public:
	struct SkyboxUniformBufferObject
	{
		glm::mat4 modelView;
		glm::mat4 proj;
	};

	void CreateSkyboxShader();

	void CreateSkyboxUniformBuffers();
	void UpdateSkyboxUniformBuffer(UINT uiIdx);

	void CreateSkyboxDescriptorSetLayout();
	void CreateSkyboxDescriptorPool();
	void CreateSkyboxDescriptorSets();

	void CreateSkyboxGraphicPipelineLayout();
	void CreateSkyboxGraphicPipeline();

public:
	struct MeshGridUniformBufferObject
	{
		glm::mat4 model;
		glm::mat4 view;
		glm::mat4 proj;
	};

	void CalcMeshGridVertexData();
	void CalcMeshGridIndexData();

	void RecreateMeshGridVertexBuffer();
	void RecreateMeshGridIndexBuffer();

	void CreateMeshGridVertexBuffer();
	void CreateMeshGridIndexBuffer();

	void CreateMeshGridShader();

	void CreateMeshGridUniformBuffers();
	void UpdateMeshGridUniformBuffer(UINT uiIdx);

	void CreateMeshGridDescriptorSetLayout();
	void CreateMeshGridDescriptorPool();
	void CreateMeshGridDescriptorSets();

	void CreateMeshGridGraphicPipelineLayout();
	void CreateMeshGridGraphicPipeline();

public:
	void CreateEllipseVertexBuffer();
	void CreateEllipseIndexBuffer();

	void CreateEllipseUniformBuffers();
	void UpdateEllipseUniformBuffer(UINT uiIdx);

	void CreateEllipseDescriptorSetLayout();
	void CreateEllipseDescriptorPool();
	void CreateEllipseDescriptorSets();

	void CreateEllipseGraphicPipelineLayout();
	void CreateEllipseGraphicPipeline();

public:
	struct BlinnPhongMVPUniformBufferObject
	{
		glm::mat4 model;
		glm::mat4 view;
		glm::mat4 proj;
		glm::mat4 mv_normal; //用于将normal转到视图空间
	};

	struct BlinnPhongLightUniformBufferObject
	{
		alignas(16) glm::vec3 position;
		alignas(16) glm::vec4 ambient;
		alignas(16) glm::vec4 diffuse;
		alignas(16) glm::vec4 specular;

		alignas(4) float intensify;
		//Attenuation
		alignas(4) float constant;
		alignas(4) float linear;
		alignas(4) float quadratic;
	};

	struct BlinnPhongMaterialUniformBufferObject
	{
		alignas(16) glm::vec4 ambient;
		alignas(16) glm::vec4 diffuse;
		alignas(16) glm::vec4 specular;
		alignas(4)  float shininess;
	};

	void InitBlinnPhongLightMaterialInfo();

	void CreateBlinnPhongShaderModule();

	void CreateBlinnPhongMVPUniformBuffers();
	void UpdateBlinnPhongMVPUniformBuffer(UINT uiIdx);

	void CreateBlinnPhongLightUniformBuffers();
	void UpdateBlinnPhongLightUniformBuffer(UINT uiIdx);

	void CreateBlinnPhongMaterialUniformBuffers();
	void UpdateBlinnPhongMaterialUniformBuffer(UINT uiIdx);

	void CreateBlinnPhongDescriptorSetLayout();
	void CreateBlinnPhongDescriptorPool();
	void CreateBlinnPhongDescriptorSets();

	void CreateBlinnPhongGraphicPipelineLayout();
	void CreateBlinnPhongGraphicPipeline();


private:
	UINT m_uiWindowWidth;
	UINT m_uiWindowHeight;
	UINT m_uiPrimaryMonitorWidth;
	UINT m_uiPrimaryMonitorHeight;
	std::string m_strWindowTitle;
	bool m_bFrameBufferResized;
	
	GLFWwindow* m_pWindow;

	std::vector<const char*> m_vecChosedExtensions;
	std::vector<VkExtensionProperties> m_vecValidExtensions;

	bool m_bEnableValidationLayer;
	std::vector<const char*> m_vecChosedValidationLayers;
	std::vector<VkLayerProperties> m_vecValidValidationLayers;

	VkDebugUtilsMessengerEXT m_DebugMessenger;

	VkInstance m_Instance;

	VkSurfaceKHR m_WindowSurface;

	std::vector<VkPhysicalDevice> m_vecValidPhysicalDevices;
	std::unordered_map<VkPhysicalDevice, PhysicalDeviceInfo> m_mapPhysicalDeviceInfo;
	VkPhysicalDevice m_PhysicalDevice;

	VkDevice m_LogicalDevice;
	const std::vector<const char*> m_vecDeviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	};
	VkQueue m_GraphicQueue;
	VkQueue m_PresentQueue;

	VkSwapchainKHR m_SwapChain = VK_NULL_HANDLE;
	VkSurfaceFormatKHR m_SwapChainSurfaceFormat;
	VkFormat m_SwapChainFormat;
	VkPresentModeKHR m_SwapChainPresentMode;
	VkExtent2D m_SwapChainExtent2D;
	UINT m_uiSwapChainMinImageCount;

	std::vector<VkImage> m_vecSwapChainImages;
	std::vector<VkImageView> m_vecSwapChainImageViews;

	VkImage m_DepthImage;
	VkDeviceMemory m_DepthImageMemory;
	VkImageView m_DepthImageView;
	VkFormat m_DepthFormat;
	std::vector<VkFramebuffer> m_vecSwapChainFrameBuffers;

	VkRenderPass m_RenderPass;

	VkCommandPool m_TransferCommandPool;

	std::unordered_map<VkShaderStageFlagBits, std::filesystem::path> m_mapShaderPath;
	std::unordered_map<VkShaderStageFlagBits, VkShaderModule> m_mapShaderModule;

	std::vector<VkBuffer> m_vecUniformBuffers;
	std::vector<VkDeviceMemory> m_vecUniformBufferMemories;
	UniformBufferObject m_UboData;
	size_t m_UboBufferSize;

	DZW_VulkanWrap::Texture m_Texture;

	DZW_VulkanWrap::Model m_Model;

	VkDescriptorSetLayout m_DescriptorSetLayout;
	VkDescriptorPool m_DescriptorPool;
	std::vector<VkDescriptorSet> m_vecDescriptorSets;

	VkCommandPool m_CommandPool;
	std::vector<VkCommandBuffer> m_vecCommandBuffers;

	VkPipelineLayout m_GraphicPipelineLayout;
	VkPipeline m_GraphicPipeline;

	std::vector<VkSemaphore> m_vecImageAvailableSemaphores;
	std::vector<VkSemaphore> m_vecRenderFinishedSemaphores;
	std::vector<VkFence> m_vecInFlightFences;

	UINT m_uiCurFrameIdx;

	bool m_bNeedResize = false;

	//Dynamic Uniform
	std::vector<VkBuffer> m_vecDynamicUniformBuffers;
	std::vector<VkDeviceMemory> m_vecDynamicUniformBufferMemories;
	size_t m_DynamicAlignment;
	DynamicUniformBufferObject m_DynamicUboData;
	size_t m_DynamicUboBufferSize;

	//Skybox
	bool m_bEnableSkybox = true;
	float m_fSkyboxRotateSpeed = 1.f;
	DZW_VulkanWrap::Texture m_SkyboxTexture;
	DZW_VulkanWrap::Model m_SkyboxModel;
	std::unordered_map<VkShaderStageFlagBits, VkShaderModule> m_mapSkyboxShaderModule;
	SkyboxUniformBufferObject m_SkyboxUboData;
	std::vector<VkBuffer> m_vecSkyboxUniformBuffers;
	std::vector<VkDeviceMemory> m_vecSkyboxUniformBufferMemories;
	VkDescriptorSetLayout m_SkyboxDescriptorSetLayout;
	VkDescriptorPool m_SkyboxDescriptorPool;
	std::vector<VkDescriptorSet> m_vecSkyboxDescriptorSets;
	VkPipelineLayout m_SkyboxGraphicPipelineLayout;
	VkPipeline m_SkyboxGraphicPipeline;

	//Mesh Grid
	bool m_bEnableMeshGrid = false;
	float m_fMeshGridSize = 500.0f;
	float m_fMeshGridSplit = 10.f;
	float m_fMeshGridLineWidth = 2.f;
	float m_fLastMeshGridSplit = m_fMeshGridSplit;
	float m_fLastMeshGridSize = m_fMeshGridSize;
	std::vector<Vertex3D> m_vecMeshGridVertices;
	std::vector<UINT> m_vecMeshGridIndices;
	std::unordered_map<VkShaderStageFlagBits, VkShaderModule> m_mapMeshGridShaderModule;
	MeshGridUniformBufferObject m_MeshGridUboData;
	std::vector<VkBuffer> m_vecMeshGridUniformBuffers;
	std::vector<VkDeviceMemory> m_vecMeshGridUniformBufferMemories;
	VkBuffer m_MeshGridVertexBuffer = VK_NULL_HANDLE;
	VkDeviceMemory m_MeshGridVertexBufferMemory = VK_NULL_HANDLE;
	VkBuffer m_MeshGridIndexBuffer;
	VkDeviceMemory m_MeshGridIndexBufferMemory;
	VkDescriptorSetLayout m_MeshGridDescriptorSetLayout;
	VkDescriptorPool m_MeshGridDescriptorPool;
	std::vector<VkDescriptorSet> m_vecMeshGridDescriptorSets;
	VkPipelineLayout m_MeshGridGraphicPipelineLayout;
	VkPipeline m_MeshGridGraphicPipeline;

	//ellipse
	bool m_bEnableEllipse = false;
	DZW_MathWrap::Ellipse m_Ellipse = DZW_MathWrap::Ellipse({0.f, 0.f, 0.f}, 1.f, 0.5f, 1.f);
	MeshGridUniformBufferObject m_EllipseUboData;
	std::vector<VkBuffer> m_vecEllipseUniformBuffers;
	std::vector<VkDeviceMemory> m_vecEllipseUniformBufferMemories;
	VkBuffer m_EllipseVertexBuffer;
	VkDeviceMemory m_EllipseVertexBufferMemory;
	VkBuffer m_EllipseIndexBuffer;
	VkDeviceMemory m_EllipseIndexBufferMemory;
	VkDescriptorSetLayout m_EllipseDescriptorSetLayout;
	VkDescriptorPool m_EllipseDescriptorPool;
	std::vector<VkDescriptorSet> m_vecEllipseDescriptorSets;
	VkPipelineLayout m_EllipseGraphicPipelineLayout;
	VkPipeline m_EllipseGraphicPipeline;

	//Blinn-Phong
	bool m_bEnableBlinnPhong = true;
	DZW_MaterialWrap::BlinnPhongMaterial m_BlinnPhongMaterial;
	DZW_LightWrap::BlinnPhongPointLight m_BlinnPhongPointLight;

	DZW_VulkanWrap::Model m_BlinnPhongModel;

	std::unordered_map<VkShaderStageFlagBits, VkShaderModule> m_mapBlinnPhongShaderModule;

	BlinnPhongMVPUniformBufferObject m_BlinnPhongMVPUBOData;
	std::vector<VkBuffer> m_vecBlinnPhongMVPUniformBuffers;
	std::vector<VkDeviceMemory> m_vecBlinnPhongMVPUniformBufferMemories;

	BlinnPhongLightUniformBufferObject m_BlinnPhongLightUBOData;
	std::vector<VkBuffer> m_vecBlinnPhongLightUniformBuffers;
	std::vector<VkDeviceMemory> m_vecBlinnPhongLightUniformBufferMemories;

	BlinnPhongMaterialUniformBufferObject m_BlinnPhongMaterialUBOData;
	std::vector<VkBuffer> m_vecBlinnPhongMaterialUniformBuffers;
	std::vector<VkDeviceMemory> m_vecBlinnPhongMaterialUniformBufferMemories;

	VkDescriptorSetLayout m_BlinnPhongDescriptorSetLayout;
	VkDescriptorPool m_BlinnPhongDescriptorPool;
	std::vector<VkDescriptorSet> m_vecBlinnPhongDescriptorSets;

	VkPipelineLayout m_BlinnPhongGraphicPipelineLayout;
	VkPipeline m_BlinnPhongGraphicPipeline;
};