#ifndef VK_ENGINE_H
#define VK_ENGINE_H

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
#include "SDL.h"
#include "SDL_vulkan.h"
#include "vk_mem_alloc.h"
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_vulkan.h"

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
	if (FN() != ENGINE_SUCCESS) { \
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

struct DeletionQueue {
	std::deque<std::function<void()>> deletors;

	void push_function(std::function<void()>&& fn) {
		deletors.push_back(fn);
	}

	void flush() {
		for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
			(*it)();
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
};

constexpr unsigned int FRAME_OVERLAP = 2;

/*---------------------------
 | VULKANENGINE CLASS
 ---------------------------*/

class VulkanEngine {
public:
	EngineResult init(SDL_Window *win);
	void deinit();

	EngineResult draw();

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

	InstanceDispatch instanceDispatch;
	EngineResult load_instance_pfns();

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
	EngineResult create_swapchain();

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
	VkExtent2D drawExtent;

	void draw_background(VkCommandBuffer cmd);

	EngineResult draw_imgui(VkCommandBuffer cmd, VkImageView targetImgView);

};

#endif /* VK_ENGINE_H */
