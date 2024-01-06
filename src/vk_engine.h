#ifndef VK_ENGINE_H
#define VK_ENGINE_H

#include <vulkan/vulkan.h>

#include "os.h"
#include "vk_dispatch.h"

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
};

#endif /* VK_ENGINE_H */
