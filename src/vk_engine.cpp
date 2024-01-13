#include "vk_engine.h"

#if defined(VK_USE_PLATFORM_WIN32_KHR)
	#define load_proc_addr GetProcAddress
#elif defined(VK_USE_PLATFORM_XCB_KHR)
	#define load_proc_addr dlsym
#endif

EngineResult VulkanEngine::init(SDL_Window *win) {
	window = win;

	ENGINE_RUN_FN(link);
	ENGINE_RUN_FN(create_instance);
	ENGINE_RUN_FN(create_surface);
	ENGINE_RUN_FN(select_physical_device);
	ENGINE_RUN_FN(create_device);

	return ENGINE_SUCCESS;
}

void VulkanEngine::deinit() {
	if (dev != NULL) {
		ENGINE_MESSAGE("Destroying logical device.");
		ddisp.vkDestroyDevice(dev, NULL);
	}

	if (surf != NULL) {
		ENGINE_MESSAGE("Destroying surface.");
		idisp.vkDestroySurfaceKHR(inst, surf, NULL);
	}

	if (inst != NULL) {
		ENGINE_MESSAGE("Destroying instance.");
		idisp.vkDestroyInstance(inst, NULL);
	}

	if (lib != nullptr) {
		ENGINE_MESSAGE("Unloading the Vulkan loader.");
#if defined(VK_USE_PLATFORM_XCB_KHR)
		dlclose(lib);
#elif defined(VK_USE_PLATFORM_WIN32_KHR)
		FreeLibrary(lib);
#endif
	}
}

EngineResult VulkanEngine::link() {
#if defined(VK_USE_PLATFORM_WIN32_KHR)
	lib = LoadLibrary("vulkan-1.dll");
#elif defined(VK_USE_PLATFORM_XCB_KHR)
	lib = dlopen("libvulkan.so.1", RTLD_NOW);
#endif

	if (lib == nullptr) {
		ENGINE_ERROR("Could not find Vulkan library. Please make sure you have a vulkan compatible graphics driver installed.")
		return ENGINE_FAILURE;
	}	

	PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)load_proc_addr(lib, "vkGetInstanceProcAddr");
	if (!idisp.vkGetInstanceProcAddr) {
		ENGINE_ERROR("Could not load vkGetInstanceProcAddr.");
		return ENGINE_FAILURE;
	}

	ENGINE_MESSAGE("Successfully loaded the Vulkan loader.");

	// First time calling this function for global level instance functions
	load_instance_dispatch_table(&idisp, vkGetInstanceProcAddr, NULL);

	return ENGINE_SUCCESS;
}

EngineResult VulkanEngine::create_instance() {
	unsigned int sdl_ext_count = 0;
	SDL_Vulkan_GetInstanceExtensions(window, &sdl_ext_count, NULL);
	std::vector<const char*> exts(sdl_ext_count);
	if (SDL_Vulkan_GetInstanceExtensions(window, &sdl_ext_count, exts.data()) != SDL_TRUE) {
		return ENGINE_FAILURE;
	}

	// This is commented out because SDL will automatically get VK_KHR_surface...
	// uncomment if need extra extensions that SDL doesn't fetch automatically
	/*for (int i = 0; i < vk_inst_extensions.size(); i++) {
		exts.push_back(vk_inst_extensions.at(i));
	}*/

	ENGINE_MESSAGE("Attempting to create instance with the following extensions:");
	for (int i = 0; i < exts.size(); i++) {
		fprintf(stderr, "\t%s\n", exts.at(i));
	}	

	VkInstanceCreateInfo ci;
	ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	ci.pNext = NULL;
	ci.flags = {};
	ci.pApplicationInfo = NULL;
	ci.enabledLayerCount = 0;
	ci.ppEnabledLayerNames = NULL;
	ci.enabledExtensionCount = exts.size();
	ci.ppEnabledExtensionNames = exts.data();

	if (idisp.vkCreateInstance(&ci, NULL, &inst) != VK_SUCCESS) {
		ENGINE_ERROR("Unable to create Vulkan instance.");
		return ENGINE_FAILURE;
	}

	// Second time calling this function for dispatch level instance functions
	load_instance_dispatch_table(&idisp, NULL, inst);

	return ENGINE_SUCCESS;
}

uint32_t VulkanEngine::device_suitable(VkPhysicalDevice dev) {
	fmts.clear();
	present_modes.clear();

	VkPhysicalDeviceProperties2 devprops = {};
	devprops.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	idisp.vkGetPhysicalDeviceProperties2(dev, &devprops);
	VkPhysicalDeviceFeatures devfeats = {};
	idisp.vkGetPhysicalDeviceFeatures(dev, &devfeats);

	ENGINE_MESSAGE_ARGS("Found device %s: ", devprops.properties.deviceName);

	// Extension support
	uint32_t ext_count;
	idisp.vkEnumerateDeviceExtensionProperties(dev, NULL, &ext_count, NULL);
	if (ext_count == 0) {
		ENGINE_WARNING("No extension support detected.");
		return 0;
	}
	VkExtensionProperties *exts = (VkExtensionProperties *)malloc(ext_count * sizeof(VkExtensionProperties));
	if (idisp.vkEnumerateDeviceExtensionProperties(dev, NULL, &ext_count, exts) != VK_SUCCESS) {
		ENGINE_WARNING("Failed to fetch extension properties.");
		return 0;
	}

	for (int i = 0; i < vk_dev_extensions.size(); i++) {
		uint32_t ext_found = 0;
		for (int j = 0; i < ext_count; j++) {
			if (!strcmp(vk_dev_extensions[i], exts[j].extensionName)) {
				ext_found = 1;
				break;
			}
		}
		if (!ext_found) {
			ENGINE_WARNING("Device does not support requested extensions.");
			return 0;
		}
	}

	// Surface formats support
	uint32_t fmts_count = 0;
	idisp.vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surf, &fmts_count, NULL);
	if (fmts_count == 0) {
		ENGINE_WARNING("No surface formats detected.");
		return 0;
	}
	if (idisp.vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surf, &fmts_count, fmts.data())
		!= VK_SUCCESS) {
		ENGINE_WARNING("Failed to fetch physical device surface formats.");
		return 0;
	}

	// Surface present modes support
	uint32_t present_modes_count = 0;
	idisp.vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surf, &present_modes_count, NULL);
	if (present_modes_count == 0) {
		ENGINE_WARNING("No surface present modes detected.");
		return 0;
	}
	if (idisp.vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surf, &present_modes_count, present_modes.data())
		!= VK_SUCCESS) {
		ENGINE_WARNING("Failed to fetch physical device surface present modes.");
		return 0;
	}

	// Surface capabilities
	if (idisp.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev, surf, &surfcaps) != VK_SUCCESS) {
		ENGINE_WARNING("Could not query for physical device surface capabilities.\n");
		return 0;
	}

	return devprops.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
		devfeats.geometryShader;
}

EngineResult VulkanEngine::select_physical_device() {
	ENGINE_MESSAGE("Finding a suitable physical device...");
	uint32_t numdevs;
	idisp.vkEnumeratePhysicalDevices(inst, &numdevs, NULL);

	if (numdevs == 0) {
		ENGINE_ERROR("Could not find any devices.");
		return ENGINE_FAILURE;
	}

	VkPhysicalDevice *devs = (VkPhysicalDevice*)malloc(numdevs * sizeof(VkPhysicalDevice));
	if (idisp.vkEnumeratePhysicalDevices(inst, &numdevs, devs) != VK_SUCCESS) {
		ENGINE_ERROR("Failed to enumerate physical devices.");
		free(devs);
		return ENGINE_FAILURE;
	}

	// Finds first suitable device which may not be the best
	for (int i = 0; i < numdevs; i++) {
		if (device_suitable(devs[i])) {
			physdev = devs[i];
			break;
		}
	}
	free(devs);

	if (physdev == NULL) {
		ENGINE_ERROR("Failed to find a suitable GPU.");
		return ENGINE_FAILURE;
	}

	return ENGINE_SUCCESS;
}

static int queue_families_complete(queue_family_indices *qfi) {
	return qfi->graphicsFamily > -1;
}

EngineResult VulkanEngine::find_queue_families(VkPhysicalDevice physdev, queue_family_indices *pQfi) {
	queue_family_indices qfi;
	uint32_t qfc = 0;
	
	idisp.vkGetPhysicalDeviceQueueFamilyProperties(physdev, &qfc, NULL);
	if (qfc == 0) {
		ENGINE_ERROR("Could not detect any queue families for device.");
		return ENGINE_FAILURE;
	}
	VkQueueFamilyProperties *qfs = (VkQueueFamilyProperties*)malloc(qfc * sizeof(VkQueueFamilyProperties));
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
		ENGINE_ERROR("Could not find graphics queue family.");
		return ENGINE_FAILURE;
		
	}

	memcpy(pQfi, &qfi, sizeof(queue_family_indices));
	return ENGINE_SUCCESS;
}

EngineResult VulkanEngine::create_device() {
	queue_family_indices qfi = {};
	if (find_queue_families(physdev, &qfi) != ENGINE_SUCCESS) {
		return ENGINE_FAILURE;
	}

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
	ci.enabledExtensionCount = vk_dev_extensions.size();
	ci.ppEnabledExtensionNames = vk_dev_extensions.data();
	ci.pEnabledFeatures = NULL;

	if (idisp.vkCreateDevice(physdev, &ci, NULL, &dev) != VK_SUCCESS) {
		fprintf(stderr, "[VulkanEngine] Could not create logical device.");
		ENGINE_ERROR("Could not create logical device.");
		return ENGINE_FAILURE;
	};
	
	load_device_dispatch_table(&ddisp, idisp.vkGetInstanceProcAddr, inst, dev);
	ENGINE_MESSAGE("Created device and loaded device dispatch table.");

	return ENGINE_SUCCESS;
}

EngineResult VulkanEngine::create_surface() {
	if (SDL_Vulkan_CreateSurface(window, inst, &surf) != SDL_TRUE) {
		ENGINE_ERROR("Unable to create Vulkan surface from SDL window.");
		return ENGINE_FAILURE;
	}
	return ENGINE_SUCCESS;
}

/*EngineResult VulkanEngine::create_swapchain() {
	VkSwapchainCreateInfoKHR ci;

	if (ddisp->vkCreateSwapchainKHR(#device, #pCreateInfo, #pAllocator, &swapchain) != VK_SUCCESS) {
		ENGINE_ERROR("Could not create swapchain.");
		return ENGINE_FAILURE;
	}
	return ENGINE_SUCCESS;
}*/
