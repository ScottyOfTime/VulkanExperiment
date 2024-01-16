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

const std::array<const char*, 1> vkInstanceExtensions = {
	"VK_KHR_surface"
};

const std::array<const char*, 1> vkDeviceExtensions = {
	"VK_KHR_swapchain"
};

struct QueueFamilyIndices {
	int32_t graphicsFamily = -1;
	int32_t presentFamily = -1;
};

struct FrameData {
	VkCommandPool cmdPool = NULL;
	VkCommandBuffer cmdBuf;
};

constexpr unsigned int FRAME_OVERLAP = 2;

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
	EngineResult init_commands();
};

#endif /* VK_ENGINE_H */
