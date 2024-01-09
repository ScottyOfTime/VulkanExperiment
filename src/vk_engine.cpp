#include "vk_engine.h"

#include <stdio.h>

#if defined(VK_USE_PLATFORM_WIN32_KHR)
	#define load_proc_addr GetProcAddress
#elif defined(VK_USE_PLATFORM_XCB_KHR)
	#define load_proc_addr dlsym
#endif

void VulkanEngine::init() {
	link();
	create_instance();
	select_physical_device();
	create_device();
}

void VulkanEngine::deinit() {
	ddisp.vkDestroyDevice(dev, NULL);

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

	PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)load_proc_addr(lib, "vkGetInstanceProcAddr");
	if (!idisp.vkGetInstanceProcAddr) {
		fprintf(stderr, "[VulkanEngine] Could not load vkGetInstanceProcAddr.\n");
		return -1;
	}
	fprintf(stderr, "[VulkanEngine] Loaded vkGetInstanceProcAddr\n");

	fprintf(stderr, "[VulkanEngine] Successfully loaded the Vulkan loader.\n");

	// First time calling this function for global level instance functions
	load_instance_dispatch_table(&idisp, vkGetInstanceProcAddr, NULL);

	return 0;
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

	if (idisp.vkCreateInstance(&ci, NULL, &inst) != VK_SUCCESS) {
		fprintf(stderr, "[VulkanEngine] Unable to create Vulkan instance.\n");
	}

	// Second time calling this function for dispatch level instance functions
	load_instance_dispatch_table(&idisp, NULL, inst);
}

static int device_suitable(VkPhysicalDevice dev, instance_dispatch* idisp) {
	VkPhysicalDeviceProperties2 devprops = {};
	devprops.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	idisp->vkGetPhysicalDeviceProperties2(dev, &devprops);
	VkPhysicalDeviceFeatures devfeats = {};
	idisp->vkGetPhysicalDeviceFeatures(dev, &devfeats);

	fprintf(stderr, "[VulkanEngine] Found device %s: ", devprops.properties.deviceName);

	return devprops.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
		devfeats.geometryShader;
}

void VulkanEngine::select_physical_device() {
	fprintf(stderr, "[VulkanEngine] Finding suitable physical device...\n");
	uint32_t numdevs;
	idisp.vkEnumeratePhysicalDevices(inst, &numdevs, NULL);
	if (numdevs == 0) {
		fprintf(stderr, "[VulkanEngine] Could not find any devices.\n");
		exit(1);
	}
	VkPhysicalDevice* devs = (VkPhysicalDevice*)malloc(numdevs * sizeof(VkPhysicalDevice));
	idisp.vkEnumeratePhysicalDevices(inst, &numdevs, devs);

	// Finds first suitable device which may not be the best
	for (int i = 0; i < numdevs; i++) {
		if (device_suitable(devs[i], &idisp)) {
			fprintf(stderr, "SUITABLE.\n");
			physdev = devs[i];
			break;
		} else {
			fprintf(stderr, "UNSUITABLE.\n");
		}
	}
	free(devs);

	if (physdev == NULL) {
		fprintf(stderr, "[VulkanEngine] Failed to find a suitable GPU.\n");
		exit(1);
	}
}

static int queue_families_complete(queue_family_indices *qfi) {
	return qfi->graphicsFamily > -1;
}

queue_family_indices VulkanEngine::find_queue_families(VkPhysicalDevice physdev) {
	queue_family_indices qfi;
	uint32_t qfc = 0;
	
	idisp.vkGetPhysicalDeviceQueueFamilyProperties(physdev, &qfc, NULL);
	if (qfc == 0) {
		fprintf(stderr, "[VulkanEngine] Could not detect any queue families for device.\n");
		exit(1);
	}
	VkQueueFamilyProperties* qfs = (VkQueueFamilyProperties*)malloc(qfc * sizeof(VkQueueFamilyProperties));
	idisp.vkGetPhysicalDeviceQueueFamilyProperties(physdev, &qfc, qfs);

	for (int i = 0; i < qfc; i++) {
		if (queue_families_complete(&qfi)) {
			break;
		}
		VkQueueFamilyProperties q = qfs[i];
		if (q.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			qfi.graphicsFamily = i;
		}
	}
	free (qfs);

	if (qfi.graphicsFamily == -1) {
		fprintf(stderr, "[VulkanEngine] Could not find graphics queue family.\n");
	}

	return qfi;
}

void VulkanEngine::create_device() {
	queue_family_indices qfi = find_queue_families(physdev);

	VkDeviceQueueCreateInfo qci;
	qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	qci.pNext = NULL;
	qci.flags = {};
	qci.queueFamilyIndex = qfi.graphicsFamily;
	qci.queueCount = 1;
	float qp = 1.0f;
	qci.pQueuePriorities = &qp;

	VkDeviceCreateInfo ci;
	ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	ci.pNext = NULL;
	ci.flags = {};
	ci.queueCreateInfoCount = 1;
	ci.pQueueCreateInfos = &qci;
	ci.enabledLayerCount = 0;
	ci.enabledExtensionCount = 0;
	ci.ppEnabledExtensionNames = NULL;
	ci.pEnabledFeatures = NULL;

	if (idisp.vkCreateDevice(physdev, &ci, NULL, &dev) != VK_SUCCESS) {
		fprintf(stderr, "[VulkanEngine] Could not create logical device.");
		exit(1);
	};
	load_device_dispatch_table(&ddisp, idisp.vkGetInstanceProcAddr, inst, dev);
	fprintf(stderr, "[VulkanEngine] Created device and loaded device dispatch table.\n");
}
