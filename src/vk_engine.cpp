#include "vk_engine.h"

#include <stdio.h>

#if defined(VK_USE_PLATFORM_WIN32_KHR)
	#define load_proc_addr GetProcAddress
#elif defined(VK_USE_PLATFORM_XCB_KHR)
	#define load_proc_addr dlsym
#endif

void VulkanEngine::init() {
	link();
	load_instance_pfns();
	create_instance();
}

void VulkanEngine::deinit() {
	idisp.vkDestroyInstance(inst, NULL);

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
		fprintf(stderr, "[VulkanEngine] Could not find Vulkan library. Please make sure you have a vulkan compatible graphics driver installed.\n");
		return -1;
	}	

	idisp.vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)load_proc_addr(lib, "vkGetInstanceProcAddr");
	if (!idisp.vkGetInstanceProcAddr) {
		fprintf(stderr, "[VulkanEngine] Could not load vkGetInstanceProcAddr.\n");
		return -1;
	}
	fprintf(stderr, "[VulkanEngine] Loaded vkGetInstanceProcAddr\n");

	fprintf(stderr, "[VulkanEngine] Successfully loaded the Vulkan loader.\n");
	return 0;
}

void VulkanEngine::load_instance_pfns() {
	idisp.vkCreateInstance = (PFN_vkCreateInstance)load_proc_addr(lib, "vkCreateInstance");
	idisp.vkDestroyInstance = (PFN_vkDestroyInstance)load_proc_addr(lib, "vkDestroyInstance");
}

void VulkanEngine::create_instance() {
	VkInstanceCreateInfo ci;
	ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	ci.pNext = NULL;
	ci.flags = {};
	ci.pApplicationInfo = NULL;
	ci.enabledLayerCount = 0;
	ci.ppEnabledLayerNames = NULL;
	ci.enabledExtensionCount = 0;
	ci.ppEnabledExtensionNames = NULL;

	idisp.vkCreateInstance(&ci, NULL, &inst);
}
