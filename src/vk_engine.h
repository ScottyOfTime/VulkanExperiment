#ifndef VK_ENGINE_H
#define VK_ENGINE_H

// #define ENGINE_VERBOSE

#include <stdio.h>
#include <cmath>
#include <string.h>
#include <vulkan/vulkan.h>
#include <vector>
#include <array>
#include <set>
#include <deque>
#include <functional>

#include "os.h"
#include "vk_dispatch.h"
#include "vk_types.h"
#include "vk_descriptors.h"
#include "vk_pipelines.h"
#include "vk_loader.h"
#include "SDL3/SDL.h"
#include "SDL3/SDL_vulkan.h"
#include "vk_mem_alloc.h"
#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_vulkan.h"
#include "glm/glm.hpp"
#include "glm/vec4.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/transform.hpp"
#include "glm/gtx/quaternion.hpp"

/*---------------------------
 | MACROS
 ---------------------------*/

#define EngineResult 	uint32_t
#define ENGINE_SUCCESS	0
#define ENGINE_FAILURE	1

#define TIMEOUT_N		1000000000

#define ENGINE_MESSAGE(MSG) \
	fprintf(stderr, "[VulkanEngine] INFO: " MSG "\n");

#define ENGINE_MESSAGE_ARGS(MSG, ...) \
	fprintf(stderr, "[VulkanEngine] INFO: " MSG "\n", ##__VA_ARGS__);

#define ENGINE_WARNING(MSG) \
	fprintf(stderr, "[VulkanEngine] WARN: " MSG "\n");

#define ENGINE_ERROR(MSG) \
	fprintf(stderr, "[VulkanEngine] ERR: " MSG "\n");

#define ENGINE_RUN_FN(FN) \
	if (FN != ENGINE_SUCCESS) { \
		return ENGINE_FAILURE; \
	}

// This is currently not in use as I like the if + return statements in code ->
// it makes it easier to parse and the engine code and find vulkan function calls
// update: I have started using this instead of the big if statement -> will need
// to refactor existing ones in order to maintain consistency
#define VK_RUN_FN(FN, MSG) \
	if (FN != VK_SUCCESS) { \
		ENGINE_ERROR(MSG); \
		return ENGINE_FAILURE; \
	}

/*---------------------------
 | CONST ARRAYS AND STRUCTS
 ---------------------------*/

const std::array<const char*, 1> vkInstanceExtensions = {
	"VK_KHR_surface"
};

const std::array<const char*, 1> vkDeviceExtensions = {
	"VK_KHR_swapchain"
};

// @Refactor -> put into own file this header is huge enough 
struct Camera {
	glm::vec3 pos;
	glm::vec3 vel;

	float pitch = 0.f;
	float yaw = 0.f;

	glm::mat4 calcViewMat() {
		glm::mat4 translation = glm::translate(glm::mat4(1.f), pos);
		glm::mat4 rotation = calcRotationMat();
		return glm::inverse(translation * rotation);
	}
	glm::mat4 calcRotationMat() {
		glm::quat pitchRotation = glm::angleAxis(pitch, glm::vec3 { 1.f, 0.f, 0.f });
		glm::quat yawRotation = glm::angleAxis(yaw, glm::vec3 {0.f, -1.f, 0.f});
		return glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
	}
};

struct DeletionQueue {
	// this struct is for debugging purposes so you can tell
	// what function is being called per iteration in flush()
	// this will be slow but honestly using std::function is already
	// dubious so will be passed in later optimization
	struct NamedFunc {
		const char* name;
		std::function<void()> func;
	};
	std::deque<NamedFunc> deletors;

	void push_function(const char* name, std::function<void()>&& fn) {
		deletors.push_back(NamedFunc{
			.name = name,
			.func = std::move(fn)
		});
	}

	void flush() {
		for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
#ifdef ENGINE_VERBOSE
			fprintf(stderr, "[DeletorsQueue] Calling function %s\n", it->name);
#endif
			it->func();
		}
		deletors.clear();
	}
};

struct QueueFamilyIndices {
	int32_t graphicsFamily = -1;
	int32_t presentFamily = -1;
};

struct FrameData {
	VkCommandPool cmdPool = NULL;
	VkCommandBuffer cmdBuf;

	VkSemaphore swapchainSemaphore = NULL,
		renderSemaphore = NULL;
	VkFence renderFence = NULL;

	DeletionQueue deletionQueue;
	DescriptorAllocatorGrowable frameDescriptors;
};

struct ComputePushConstants {
	glm::vec4 data1;
	glm::vec4 data2;
	glm::vec4 data3;
	glm::vec4 data4;
};

struct ComputeEffect {
	const char *name;

	VkPipeline pipeline;
	VkPipelineLayout layout;

	ComputePushConstants data;
};

struct GPUSceneData {
	glm::mat4 view;
	glm::mat4 proj;
	glm::mat4 viewproj;
	glm::vec4 ambientColor;
	glm::vec4 sunlightDirection;
	glm::vec4 sunlightColor;
};

constexpr unsigned int FRAME_OVERLAP = 2;

/*---------------------------
 | VULKANENGINE CLASS
 ---------------------------*/

class VulkanEngine {
public:
	EngineResult init(SDL_Window *win);
	void deinit();

	VkExtent2D windowExtent{ 1280, 720 };

	EngineResult draw();
	float renderScale = 1.f;

	bool resizeRequested = false;
	EngineResult resize_swapchain();

	DescriptorAllocator descriptorAllocator;
	VkDescriptorSet drawImageDescriptors;
	VkDescriptorSetLayout drawImageDescriptorLayout;

	VkPipeline gradientPipeline;
	VkPipelineLayout gradientPipelineLayout;

	// Immediate mode submit structures
	VkFence immFence;
	VkCommandBuffer immCmdBuf;
	VkCommandPool immCmdPool;

	EngineResult immediate_submit(std::function<void(VkCommandBuffer)>&& fn);


	std::vector<ComputeEffect> backgroundEffects;
	int currentBackgroundEffect{ 0 };

	GPUMeshBuffers upload_mesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

	Camera camera = { 
		.pos = glm::vec3{ 0, 0, -3 },
		.vel = glm::vec3{ 0, 0, 0 }
	};

private:
	VmaAllocator allocator;
	DeletionQueue mainDeletionQueue;
	SDL_Window *window;

#if defined(VK_USE_PLATFORM_XCB_KHR)
	void *lib;
#elif defined(VK_USE_PLATFORM_WIN32_KHR)
	HMODULE lib;
#endif
	EngineResult link();
	int shutdown = 0;

	InstanceDispatch instanceDispatch;

	VkInstance instance = NULL;
	EngineResult create_instance();

	VkSurfaceKHR surface = NULL;
	VkSurfaceCapabilitiesKHR surfaceCaps;
	std::vector<VkSurfaceFormatKHR> availableFormats;
	std::vector<VkPresentModeKHR> availablePresentModes;
	EngineResult create_surface();

	VkPhysicalDevice physicalDevice = NULL;
	QueueFamilyIndices queueFamilies;
	EngineResult find_queue_families(VkPhysicalDevice physdev,
			QueueFamilyIndices *qfi);
	uint32_t device_suitable(VkPhysicalDevice dev);
	EngineResult select_physical_device();

	VkDevice device = NULL;
	VkQueue graphicsQueue;
	VkQueue presentQueue;
	DeviceDispatch deviceDispatch;
	EngineResult create_device();

	VkSwapchainKHR swapchain = NULL;
	VkFormat swapchainFormat;
	std::vector<VkImage> swapchainImgs;
	std::vector<VkImageView> swapchainImgViews;
	VkExtent2D swapchainExtent;
	EngineResult create_swapchain(uint32_t width, uint32_t height);
	EngineResult init_swapchain();
	EngineResult destroy_swapchain();

	struct FrameData frames[FRAME_OVERLAP];
	uint32_t frameNumber = 0;
	struct FrameData& get_current_frame() 
	{ 
		return frames[frameNumber % FRAME_OVERLAP]; 
	}
	EngineResult init_commands();

	EngineResult init_sync();

	EngineResult init_descriptors();

	EngineResult init_pipelines();
	EngineResult init_background_pipelines();

	EngineResult init_imgui();

	// Draw resources
	AllocatedImage drawImage;
	AllocatedImage depthImage;
	VkExtent2D drawExtent;


	void draw_background(VkCommandBuffer cmd);

	EngineResult draw_imgui(VkCommandBuffer cmd, VkImageView targetImgView);

	EngineResult draw_geometry(VkCommandBuffer cmd);

	EngineResult create_buffer(AllocatedBuffer* buffer, size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
	void destroy_buffer(const AllocatedBuffer* buffer);
	// these two values should be equal by end of program, if not
	// indicates bug
	uint32_t buffersCreated = 0;
	uint32_t buffersDestroyed = 0;


	VkPipelineLayout meshPipelineLayout;
	VkPipeline meshPipeline;

	EngineResult init_mesh_pipeline();
	// dummy mesh creation <- now being loaded from glb
	void init_default_data();

	std::vector<std::shared_ptr<MeshAsset>> testMeshes;

	// scene data and its descriptor layout
	GPUSceneData sceneData;
	VkDescriptorSetLayout gpuSceneDataDescLayout;

	// image creation and deletion
	EngineResult create_image(AllocatedImage* img, VkExtent3D size, VkFormat format, 
		VkImageUsageFlags usage, bool mipmapped = false);
	EngineResult create_image_with_data(AllocatedImage* img, void* data, VkExtent3D size, 
		VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
	void destroy_image(const AllocatedImage* img);

	// default image textures and samplers plus a descriptor set layout
	// for textured images
	AllocatedImage whiteImage;
	AllocatedImage blackImage;
	AllocatedImage greyImage;
	AllocatedImage errorCheckerboardImage;

	VkSampler defaultSamplerLinear;
	VkSampler defaultSamplerNearest;

	VkDescriptorSetLayout singleImageDescriptorLayout;
};

#endif /* VK_ENGINE_H */
