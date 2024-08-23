#include "vk_dispatch.h"

void load_instance_dispatch_table(InstanceDispatch *disp, PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr, VkInstance inst) {
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


void load_device_dispatch_table(DeviceDispatch *disp, PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr, VkInstance inst, VkDevice dev) {
	disp->vkGetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)vkGetInstanceProcAddr(inst, "vkGetDeviceProcAddr");
	disp->vkDestroyDevice = (PFN_vkDestroyDevice)disp->vkGetDeviceProcAddr(dev, "vkDestroyDevice");
	disp->vkDeviceWaitIdle = (PFN_vkDeviceWaitIdle)disp->vkGetDeviceProcAddr(dev, "vkDeviceWaitIdle");

	disp->vkGetDeviceQueue = (PFN_vkGetDeviceQueue)disp->vkGetDeviceProcAddr(dev, "vkGetDeviceQueue");

	disp->vkCreateSwapchainKHR = (PFN_vkCreateSwapchainKHR)disp->vkGetDeviceProcAddr(dev, "vkCreateSwapchainKHR");
	disp->vkDestroySwapchainKHR = (PFN_vkDestroySwapchainKHR)disp->vkGetDeviceProcAddr(dev, "vkDestroySwapchainKHR");

	disp->vkGetSwapchainImagesKHR = (PFN_vkGetSwapchainImagesKHR)disp->vkGetDeviceProcAddr(dev, "vkGetSwapchainImagesKHR");

	disp->vkCreateImageView = (PFN_vkCreateImageView)disp->vkGetDeviceProcAddr(dev, "vkCreateImageView");
	disp->vkDestroyImageView = (PFN_vkDestroyImageView)disp->vkGetDeviceProcAddr(dev, "vkDestroyImageView");

	disp->vkCreateCommandPool = (PFN_vkCreateCommandPool)disp->vkGetDeviceProcAddr(dev, "vkCreateCommandPool");
	disp->vkDestroyCommandPool = (PFN_vkDestroyCommandPool)disp->vkGetDeviceProcAddr(dev, "vkDestroyCommandPool");

	disp->vkAllocateCommandBuffers = (PFN_vkAllocateCommandBuffers)disp->vkGetDeviceProcAddr(dev, "vkAllocateCommandBuffers");

	disp->vkCreateFence = (PFN_vkCreateFence)disp->vkGetDeviceProcAddr(dev, "vkCreateFence");
	disp->vkDestroyFence = (PFN_vkDestroyFence)disp->vkGetDeviceProcAddr(dev, "vkDestroyFence");

	disp->vkCreateSemaphore = (PFN_vkCreateSemaphore)disp->vkGetDeviceProcAddr(dev, "vkCreateSemaphore");
	disp->vkDestroySemaphore = (PFN_vkDestroySemaphore)disp->vkGetDeviceProcAddr(dev, "vkDestroySemaphore");

	disp->vkWaitForFences = (PFN_vkWaitForFences)disp->vkGetDeviceProcAddr(dev, "vkWaitForFences");
	disp->vkResetFences = (PFN_vkResetFences)disp->vkGetDeviceProcAddr(dev, "vkResetFences");

	disp->vkAcquireNextImageKHR = (PFN_vkAcquireNextImageKHR)disp->vkGetDeviceProcAddr(dev, "vkAcquireNextImageKHR");

	disp->vkResetCommandBuffer = (PFN_vkResetCommandBuffer)disp->vkGetDeviceProcAddr(dev, "vkResetCommandBuffer");
	disp->vkBeginCommandBuffer = (PFN_vkBeginCommandBuffer)disp->vkGetDeviceProcAddr(dev, "vkBeginCommandBuffer");
	disp->vkEndCommandBuffer = (PFN_vkEndCommandBuffer)disp->vkGetDeviceProcAddr(dev, "vkEndCommandBuffer");

	disp->vkCmdClearColorImage = (PFN_vkCmdClearColorImage)disp->vkGetDeviceProcAddr(dev, "vkCmdClearColorImage");

	disp->vkQueueSubmit2 = (PFN_vkQueueSubmit2)disp->vkGetDeviceProcAddr(dev, "vkQueueSubmit2");
	disp->vkCmdPipelineBarrier2 = (PFN_vkCmdPipelineBarrier2)disp->vkGetDeviceProcAddr(dev, "vkCmdPipelineBarrier2");
	disp->vkQueuePresentKHR = (PFN_vkQueuePresentKHR)disp->vkGetDeviceProcAddr(dev, "vkQueuePresentKHR");

	disp->vkCreateImageView = (PFN_vkCreateImageView)disp->vkGetDeviceProcAddr(dev, "vkCreateImageView");
	disp->vkDestroyImageView = (PFN_vkDestroyImageView)disp->vkGetDeviceProcAddr(dev, "vkDestroyImageView");

	disp->vkCmdBlitImage2 = (PFN_vkCmdBlitImage2)disp->vkGetDeviceProcAddr(dev, "vkCmdBlitImage2");

	disp->vkCreateDescriptorSetLayout = (PFN_vkCreateDescriptorSetLayout)disp->vkGetDeviceProcAddr(dev, "vkCreateDescriptorSetLayout");
	disp->vkAllocateDescriptorSets = (PFN_vkAllocateDescriptorSets)disp->vkGetDeviceProcAddr(dev, "vkAllocateDescriptorSets");
	disp->vkUpdateDescriptorSets = (PFN_vkUpdateDescriptorSets)disp->vkGetDeviceProcAddr(dev, "vkUpdateDescriptorSets");
	disp->vkDestroyDescriptorSetLayout = (PFN_vkDestroyDescriptorSetLayout)disp->vkGetDeviceProcAddr(dev, "vkDestroyDescriptorSetLayout");

	disp->vkCreateDescriptorPool = (PFN_vkCreateDescriptorPool)disp->vkGetDeviceProcAddr(dev, "vkCreateDescriptorPool");
	disp->vkResetDescriptorPool = (PFN_vkResetDescriptorPool)disp->vkGetDeviceProcAddr(dev, "vkResetDescriptorPool");
	disp->vkDestroyDescriptorPool = (PFN_vkDestroyDescriptorPool)disp->vkGetDeviceProcAddr(dev, "vkDestroyDescriptorPool");

	disp->vkCreateShaderModule = (PFN_vkCreateShaderModule)disp->vkGetDeviceProcAddr(dev, "vkCreateShaderModule");
	disp->vkDestroyShaderModule = (PFN_vkDestroyShaderModule)disp->vkGetDeviceProcAddr(dev, "vkDestroyShaderModule");

	disp->vkCreatePipelineLayout = (PFN_vkCreatePipelineLayout)disp->vkGetDeviceProcAddr(dev, "vkCreatePipelineLayout");
	disp->vkDestroyPipelineLayout = (PFN_vkDestroyPipelineLayout)disp->vkGetDeviceProcAddr(dev, "vkDestroyPipelineLayout");

	disp->vkCreateComputePipelines = (PFN_vkCreateComputePipelines)disp->vkGetDeviceProcAddr(dev, "vkCreateComputePipelines");
	disp->vkDestroyPipeline = (PFN_vkDestroyPipeline)disp->vkGetDeviceProcAddr(dev, "vkDestroyPipeline");

	disp->vkCmdBindPipeline = (PFN_vkCmdBindPipeline)disp->vkGetDeviceProcAddr(dev, "vkCmdBindPipeline");
	disp->vkCmdBindDescriptorSets = (PFN_vkCmdBindDescriptorSets)disp->vkGetDeviceProcAddr(dev, "vkCmdBindDescriptorSets");
	disp->vkCmdBindIndexBuffer = (PFN_vkCmdBindIndexBuffer)disp->vkGetDeviceProcAddr(dev, "vkCmdBindIndexBuffer");
	disp->vkCmdDispatch = (PFN_vkCmdDispatch)disp->vkGetDeviceProcAddr(dev, "vkCmdDispatch");

	disp->vkCmdBeginRendering = (PFN_vkCmdBeginRendering)disp->vkGetDeviceProcAddr(dev, "vkCmdBeginRendering");
	disp->vkCmdEndRendering = (PFN_vkCmdEndRendering)disp->vkGetDeviceProcAddr(dev, "vkCmdEndRendering");

	disp->vkCmdPushConstants = (PFN_vkCmdPushConstants)disp->vkGetDeviceProcAddr(dev, "vkCmdPushConstants");

	disp->vkCreateGraphicsPipelines = (PFN_vkCreateGraphicsPipelines)disp->vkGetDeviceProcAddr(dev, "vkCreateGraphicsPipelines");

	disp->vkCmdSetViewport = (PFN_vkCmdSetViewport)disp->vkGetDeviceProcAddr(dev, "vkCmdSetViewport");
	disp->vkCmdSetScissor = (PFN_vkCmdSetScissor)disp->vkGetDeviceProcAddr(dev, "vkCmdSetScissor");

	disp->vkCmdDraw = (PFN_vkCmdDraw)disp->vkGetDeviceProcAddr(dev, "vkCmdDraw");
	disp->vkCmdDrawIndexed = (PFN_vkCmdDrawIndexed)disp->vkGetDeviceProcAddr(dev, "vkCmdDrawIndexed");

	disp->vkCmdCopyBuffer = (PFN_vkCmdCopyBuffer)disp->vkGetDeviceProcAddr(dev, "vkCmdCopyBuffer");
	disp->vkGetBufferDeviceAddress = (PFN_vkGetBufferDeviceAddress)disp->vkGetDeviceProcAddr(dev, "vkGetBufferDeviceAddress");
}
