#ifndef VK_DISPATCH_H
#define VK_DISPATCH_H

#include <vulkan/vulkan.h>

struct instance_dispatch {
	PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;

	PFN_vkCreateInstance vkCreateInstance;
	PFN_vkDestroyInstance vkDestroyInstance;

	PFN_vkCreateDevice vkCreateDevice;

	PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR;

	PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices;
	PFN_vkGetPhysicalDeviceProperties2 vkGetPhysicalDeviceProperties2;
	PFN_vkGetPhysicalDeviceFeatures vkGetPhysicalDeviceFeatures;

	PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties;

	PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
};

void load_instance_dispatch_table(instance_dispatch *disp, PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr, VkInstance inst);

struct device_dispatch {
	PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr;
	PFN_vkDestroyDevice vkDestroyDevice;
};

void load_device_dispatch_table(device_dispatch *disp, PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr, VkInstance inst, VkDevice dev);

#endif /* VK_DISPATCH_H */
