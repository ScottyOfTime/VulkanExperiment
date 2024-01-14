#ifndef VK_ENGINE_H
#define VK_ENGINE_H

#include <stdio.h>
#include <string.h>
#include <vulkan/vulkan.h>
#include <vector>
#include <array>
#include <set>

#include "os.h"
#include "vk_dispatch.h"
#include "SDL.h"
#include "SDL_vulkan.h"

/*---------------------------
 | MACROS
 ---------------------------*/

#define EngineResult 	uint32_t
#define ENGINE_SUCCESS	0
#define ENGINE_FAILURE	1

#define ENGINE_MESSAGE(MSG) \
	fprintf(stderr, "[VulkanEngine] INFO: " MSG "\n");

#define ENGINE_MESSAGE_ARGS(MSG, ...) \
	fprintf(stderr, "[VulkanEngine] INFO: " MSG "\n", __VA_ARGS__);

#define ENGINE_WARNING(MSG) \
	fprintf(stderr, "[VulkanEngine] WARN: " MSG "\n");

#define ENGINE_ERROR(MSG) \
	fprintf(stderr, "[VulkanEngine] ERR: " MSG "\n");

#define ENGINE_RUN_FN(FN) \
	if (FN() != ENGINE_SUCCESS) { \
		return ENGINE_FAILURE; \
	}

/*---------------------------
 | CONST ARRAYS AND STRUCTS
 ---------------------------*/

const std::array<const char*, 1> vk_inst_extensions = {
	"VK_KHR_surface"
};

const std::array<const char*, 1> vk_dev_extensions = {
	"VK_KHR_swapchain"
};

struct queue_family_indices {
	int32_t graphicsFamily = -1;
	int32_t presentFamily = -1;
};

/*---------------------------
 | VULKANENGINE CLASS
 ---------------------------*/

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

	VkSurfaceKHR surf = NULL;
	VkSurfaceCapabilitiesKHR surfcaps;
	std::vector<VkSurfaceFormatKHR> available_fmts;
	std::vector<VkPresentModeKHR> available_present_modes;
	EngineResult create_surface();

	VkPhysicalDevice physdev = NULL;
	EngineResult select_physical_device();
	EngineResult find_queue_families(VkPhysicalDevice physdev,
			queue_family_indices *qfi);
	uint32_t device_suitable(VkPhysicalDevice dev);

	VkDevice dev = NULL;
	VkQueue graphicsQueue;
	VkQueue presentQueue;
	device_dispatch ddisp;
	EngineResult create_device();

	VkSwapchainKHR swapchain = NULL;
	VkFormat swapchainFmt;
	std::vector<VkImage> swapchainImgs;
	std::vector<VkImageView> swapchainImgViews;
	VkExtent2D swapchainExtent;
	EngineResult create_swapchain();
};

#endif /* VK_ENGINE_H */
