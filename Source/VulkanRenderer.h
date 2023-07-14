#pragma once

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include "glm/glm.hpp"

#include "Core.h"
#include "Camera.h"

struct Vertex3D
{
	glm::vec3 pos;
	glm::vec3 color;
	glm::vec2 texCoord;

	static VkVertexInputBindingDescription GetBindingDescription()
	{
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0; //在binding array中的Idx
		bindingDescription.stride = sizeof(Vertex3D);	//所占字节数
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; //传输速率，分为vertex或instance

		return bindingDescription;
	}

	static std::array<VkVertexInputAttributeDescription, 3> GetAttributeDescriptions()
	{
		std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};
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

		return attributeDescriptions;
	}

	bool operator==(const Vertex3D& other) const
	{
		return (pos == other.pos) && (texCoord == other.texCoord) && (color == other.color);
	}
};

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

namespace std
{
	template<> struct hash<Vertex3D>
	{
		size_t operator()(Vertex3D const& vertex) const
		{
			return ((hash<glm::vec3>()(vertex.pos) ^
				(hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
				(hash<glm::vec2>()(vertex.texCoord) << 1);
		}
	};
}

class VulkanRenderer
{
public:
	VulkanRenderer();
	~VulkanRenderer();

	VulkanRenderer& operator=(const VulkanRenderer& other) { return *this; }

	void Init();
	void Loop();
	void Clean();

	void LoadOBJ(const std::filesystem::path& modelPath);
	void LoadGLTF(const std::filesystem::path& modelPath);
	void LoadModel(const std::filesystem::path& modelPath);

private:
	static void FrameBufferResizeCallBack(GLFWwindow* pWindow, int nWidth, int nHeight);
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

	VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, UINT uiMipLevel);
	void CreateSwapChainImages();
	void CreateSwapChainImageViews();
	void CreateSwapChainFrameBuffers();

	VkFormat ChooseDepthFormat(bool bCheckSamplingSupport);
	void CreateRenderPass();

	void CreateTransferCommandPool();
	VkCommandBuffer BeginSingleTimeCommand();
	void EndSingleTimeCommand(VkCommandBuffer commandBuffer);

	std::vector<char> ReadShaderFile(const std::filesystem::path& filepath);
	VkShaderModule CreateShaderModule(const std::vector<char>& vecBytecode);
	void CreateShader();

	struct UniformBufferObject
	{
		glm::mat4* model;
		glm::mat4* view;
		glm::mat4* proj;
	};

	UniformBufferObject m_DynamicUBOData;
	UINT FindSuitableMemoryTypeIndex(UINT typeFilter, VkMemoryPropertyFlags properties);

	void AllocateBufferMemory(VkMemoryPropertyFlags propertyFlags, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
	void CreateBufferAndBindMemory(VkDeviceSize deviceSize, VkBufferUsageFlags usageFlags,
		VkMemoryPropertyFlags propertyFlags, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
	void CreateUniformBuffers();


	void CreateTextureSampler();

	void AllocateImageMemory(VkMemoryPropertyFlags propertyFlags, VkImage& image, VkDeviceMemory& bufferMemory);
	void CreateImageAndBindMemory(uint32_t uiWidth, uint32_t uiHeight, uint32_t uiMipLevel,
		VkSampleCountFlagBits sampleCount, VkFormat format,
		VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
		VkImage& image, VkDeviceMemory& imageMemory);
	bool CheckFormatHasStencilComponent(VkFormat format);
	void ChangeImageLayout(VkImage image, VkFormat format, uint32_t uiMipLevel, VkImageLayout oldLayout, VkImageLayout newLayout);
	void TransferImageDataByStageBuffer(void* pData, VkDeviceSize imageSize, VkImage& image,  UINT uiWidth, UINT uiHeight);
	void CreateTextureImageAndFillData();
	void CreateTextureImageView();

	void CreateDescriptorSetLayout();
	void CreateDescriptorPool();
	void CreateDescriptorSets();


	void TransferBufferDataByStageBuffer(void* pData, VkDeviceSize imageSize, VkBuffer& buffer);

	void CreateVertexBuffer();
	void CreateIndexBuffer();

	void CreateCommandPool();
	void CreateCommandBuffer();

	void CreateGraphicPipeline();

	void CreateSyncObjects();


	void RecordCommandBuffer(VkCommandBuffer& commandBuffer, UINT uiIdx);
	void UpdateUniformBuffer(UINT uiIdx);
	void Render();

	void RecreateSwapChain();

public:
	Camera m_Camera;
	void SetupCamera();

	UINT m_uiFPS;
	UINT m_uiFrameCounter;
	UINT GetFPS() { return m_uiFPS; }


	GLFWwindow* GetWindow() { return m_pWindow; }
	VkInstance& GetInstance() { return m_Instance; }
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

private:
	UINT m_uiWindowWidth;
	UINT m_uiWindowHeight;
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

	VkSwapchainKHR m_SwapChain;
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

	std::vector<VkBuffer> m_vecDynamicUniformBuffers;
	std::vector<VkDeviceMemory> m_vecDynamicUniformBufferMemories;
	size_t m_DynamicAlignment;

	VkSampler m_TextureSampler;

	UINT m_uiMipmapLevel;
	std::filesystem::path m_TexturePath;
	VkImage m_TextureImage;
	VkDeviceMemory m_TextureImageMemory;
	VkImageView m_TextureImageView;

	VkDescriptorSetLayout m_DescriptorSetLayout;
	VkDescriptorPool m_DescriptorPool;
	std::vector<VkDescriptorSet> m_vecDescriptorSets;

	std::filesystem::path m_ModelPath;

	VkBuffer m_VertexBuffer;
	VkDeviceMemory m_VertexBufferMemory;
	std::vector<Vertex3D> m_Vertices;

	VkBuffer m_IndexBuffer;
	VkDeviceMemory m_IndexBufferMemory;
	std::vector<UINT> m_Indices;

	VkCommandPool m_CommandPool;
	std::vector<VkCommandBuffer> m_vecCommandBuffers;

	bool m_bViewportAndScissorIsDynamic;

	VkPipelineLayout m_GraphicPipelineLayout;
	VkPipeline m_GraphicPipeline;

	std::vector<VkSemaphore> m_vecImageAvailableSemaphores;
	std::vector<VkSemaphore> m_vecRenderFinishedSemaphores;
	std::vector<VkFence> m_vecInFlightFences;

	UINT m_uiCurFrameIdx;
};