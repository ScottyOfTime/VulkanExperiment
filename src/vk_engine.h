#ifndef VK_ENGINE_H
#define VK_ENGINE_H

#include <vulkan/vulkan.h>

#include "os.h"

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
};

#endif /* VK_ENGINE_H */
