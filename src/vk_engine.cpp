#include "vk_engine.h"

#include <stdio.h>

void VulkanEngine::init() {
	link();
}

void VulkanEngine::deinit() {
	fprintf(stderr, "[VulkanEngine] Unloading the Vulkan loader.\n");
#if defined(VK_USE_PLATFORM_XCB_KHR)
	dlclose(lib);
#elif defined(VK_USE_PLATFORM_WIN32_KHR)
	FreeLibrary(lib);
#endif
}

int VulkanEngine::link() {
#if defined(VK_USE_PLATFORM_WIN32_KHR)
	lib = LoadLibrary("vulkan-1.dll");
#elif defined(VK_USE_PLATFORM_XCB_KHR)
	lib = dlopen("libvulkan.so.1", RTLD_NOW);
#endif

	if (lib == nullptr) {
		fprintf(stderr, "[VulkanEngine] Could not find Vulkan library. Please makre sure you have a vulkan compatible graphics driver installed.\n");
		return -1;
	}
	fprintf(stderr, "[VulkanEngine] Successfully loaded the Vulkan loader.\n");
	return 0;
}
