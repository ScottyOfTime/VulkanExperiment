#ifndef VK_ENGINE_H
#define VK_ENGINE_H

#include <vulkan/vulkan.h>

#include "os.h"
#include "vk_dispatch.h"

struct queue_family_indices {
	int32_t graphicsFamily = -1;
};

class VulkanEngine {
public:
	void init();

	void deinit();

private:
#if defined(VK_USE_PLATFORM_XCB_KHR)
	void *lib;
#elif defined(VK_USE_PLATFORM_WIN32_KHR)
	HMODULE lib;
#endif
	int link();

	instance_dispatch idisp;
	void load_instance_pfns();

	VkInstance inst;
	void create_instance();

	VkPhysicalDevice physdev = NULL;
	void select_physical_device();
	queue_family_indices find_queue_families(VkPhysicalDevice physdev);

	VkDevice dev = NULL;
	device_dispatch ddisp;
	void create_device();
};

#endif /* VK_ENGINE_H */
