#ifndef VK_ENGINE_H
#define VK_ENGINE_H

#include <stdio.h>
#include <vulkan/vulkan.h>
#include <vector>
#include <array>

#include "os.h"
#include "vk_dispatch.h"
#include "SDL.h"
#include "SDL_vulkan.h"

#define EngineResult 	uint32_t
#define ENGINE_SUCCESS	0
#define ENGINE_FAILURE	1

#define ENGINE_MESSAGE(MSG) \
	fprintf(stderr, "[VulkanEngine] INFO: " MSG "\n");

#define ENGINE_MESSAGE_ARGS(MSG, ARGS) \
	fprintf(stderr, "[VulkanEngine] INFO: " MSG "\n", ARGS);

#define ENGINE_WARNING(MSG) \
	fprintf(stderr, "[VulkanEngine] WARN: " MSG "\n");

#define ENGINE_ERROR(MSG) \
	fprintf(stderr, "[VulkanEngine] ERR: " MSG "\n");

#define ENGINE_RUN_FN(FN) \
	if (FN() != ENGINE_SUCCESS) { \
		return ENGINE_FAILURE; \
	}

const std::array<const char*, 1> vk_inst_extensions = {
	"VK_KHR_surface"
};

const std::array<const char*, 1> vk_dev_extensions = {
	"VK_KHR_swapchain"
};

struct queue_family_indices {
	int32_t graphicsFamily = -1;
};

class VulkanEngine {
public:
	EngineResult init(SDL_Window *win);

	void deinit();

private:
	SDL_Window *window;
#if defined(VK_USE_PLATFORM_XCB_KHR)
	void *lib;
#elif defined(VK_USE_PLATFORM_WIN32_KHR)
	HMODULE lib;
#endif
	EngineResult link();

	instance_dispatch idisp;
	EngineResult load_instance_pfns();

	VkInstance inst = NULL;
	EngineResult create_instance();

	VkPhysicalDevice physdev = NULL;
	EngineResult select_physical_device();
	EngineResult find_queue_families(VkPhysicalDevice physdev,
			queue_family_indices *qfi);

	VkDevice dev = NULL;
	device_dispatch ddisp;
	EngineResult create_device();

	VkSurfaceKHR surf = NULL;
	EngineResult create_surface();

	VkSwapchainKHR swapchain;
	VkFormat swapchainFormat;
	std::vector<VkImage> swapchainImgs;
	std::vector<VkImageView> swapchainImgViews;
	VkExtent2D swapchaintExtent;
	EngineResult create_swapchain();
};

#endif /* VK_ENGINE_H */
