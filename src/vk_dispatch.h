#ifndef VK_DISPATCH_H
#define VK_DISPATCH_H

#include <vulkan/vulkan.h>

struct instance_dispatch {
	PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;

	PFN_vkCreateInstance vkCreateInstance;
	PFN_vkDestroyInstance vkDestroyInstance;
};

#endif /* VK_DISPATCH_H */
