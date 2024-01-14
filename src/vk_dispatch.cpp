#include "vk_dispatch.h"

void load_instance_dispatch_table(instance_dispatch *disp, PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr, VkInstance inst) {
	if (inst == NULL) {
		disp->vkGetInstanceProcAddr = vkGetInstanceProcAddr;
		disp->vkCreateInstance = (PFN_vkCreateInstance)disp->vkGetInstanceProcAddr(NULL, "vkCreateInstance");
	} else {
		disp->vkDestroyInstance = (PFN_vkDestroyInstance)disp->vkGetInstanceProcAddr(inst, "vkDestroyInstance");

		disp->vkCreateDevice = (PFN_vkCreateDevice)disp->vkGetInstanceProcAddr(inst, "vkCreateDevice");

		disp->vkDestroySurfaceKHR = (PFN_vkDestroySurfaceKHR)disp->vkGetInstanceProcAddr(inst, "vkDestroySurfaceKHR");

		disp->vkEnumeratePhysicalDevices = (PFN_vkEnumeratePhysicalDevices)disp->vkGetInstanceProcAddr(inst, "vkEnumeratePhysicalDevices");
		disp->vkEnumerateDeviceExtensionProperties = (PFN_vkEnumerateDeviceExtensionProperties)disp->vkGetInstanceProcAddr(inst, "vkEnumerateDeviceExtensionProperties");
		disp->vkGetPhysicalDeviceProperties2 = (PFN_vkGetPhysicalDeviceProperties2)disp->vkGetInstanceProcAddr(inst, "vkGetPhysicalDeviceProperties2");
		disp->vkGetPhysicalDeviceFeatures = (PFN_vkGetPhysicalDeviceFeatures)disp->vkGetInstanceProcAddr(inst, "vkGetPhysicalDeviceFeatures");

		disp->vkGetPhysicalDeviceQueueFamilyProperties = (PFN_vkGetPhysicalDeviceQueueFamilyProperties)disp->vkGetInstanceProcAddr(inst, "vkGetPhysicalDeviceQueueFamilyProperties");
		disp->vkGetPhysicalDeviceSurfaceSupportKHR = (PFN_vkGetPhysicalDeviceSurfaceSupportKHR)disp->vkGetInstanceProcAddr(inst, "vkGetPhysicalDeviceSurfaceSupportKHR");
		disp->vkGetPhysicalDeviceSurfaceCapabilitiesKHR = (PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)disp->vkGetInstanceProcAddr(inst, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
		disp->vkGetPhysicalDeviceSurfaceFormatsKHR = (PFN_vkGetPhysicalDeviceSurfaceFormatsKHR)disp->vkGetInstanceProcAddr(inst, "vkGetPhysicalDeviceSurfaceFormatsKHR");
		disp->vkGetPhysicalDeviceSurfacePresentModesKHR = (PFN_vkGetPhysicalDeviceSurfacePresentModesKHR)disp->vkGetInstanceProcAddr(inst, "vkGetPhysicalDeviceSurfacePresentModesKHR");
	}
}


void load_device_dispatch_table(device_dispatch *disp, PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr, VkInstance inst, VkDevice dev) {
	disp->vkGetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)vkGetInstanceProcAddr(inst, "vkGetDeviceProcAddr");
	disp->vkDestroyDevice = (PFN_vkDestroyDevice)disp->vkGetDeviceProcAddr(dev, "vkDestroyDevice");
}
