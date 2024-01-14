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
	ENGINE_RUN_FN(create_swapchain);

	return ENGINE_SUCCESS;
}

void VulkanEngine::deinit() {
	if (swapchain != NULL) {
		ENGINE_MESSAGE("Destroying swapchain.");
		ddisp.vkDestroySwapchainKHR(dev, swapchain, NULL);
	}

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
	available_fmts.clear();
	available_present_modes.clear();

	VkPhysicalDeviceProperties2 devprops = {};
	devprops.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	idisp.vkGetPhysicalDeviceProperties2(dev, &devprops);
	VkPhysicalDeviceFeatures devfeats = {};
	idisp.vkGetPhysicalDeviceFeatures(dev, &devfeats);

	ENGINE_MESSAGE_ARGS("Found device %s: ", devprops.properties.deviceName);

	// Query extension Support
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

	// Start surface formats support
	uint32_t fmts_count = 0;
	idisp.vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surf, &fmts_count, NULL);
	if (fmts_count == 0) {
		ENGINE_WARNING("No surface formats detected.");
		return 0;
	}
	available_fmts.resize(fmts_count);
	if (idisp.vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surf, &fmts_count, available_fmts.data())
		!= VK_SUCCESS) {
		ENGINE_WARNING("Failed to fetch physical device surface formats.");
		return 0;
	}

	// Start surface present modes support
	uint32_t present_modes_count = 0;
	idisp.vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surf, &present_modes_count, NULL);
	if (present_modes_count == 0) {
		ENGINE_WARNING("No surface present modes detected.");
		return 0;
	}
	available_present_modes.resize(present_modes_count);
	if (idisp.vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surf, &present_modes_count, available_present_modes.data())
		!= VK_SUCCESS) {
		ENGINE_WARNING("Failed to fetch physical device surface present modes.");
		return 0;
	}

	// Start surface capabilities
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
	return qfi->graphicsFamily > -1 && qfi->presentFamily > -1;
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
			ENGINE_MESSAGE_ARGS("Found queue families:\n\tNAME\t\tINDEX\n\tGRAPHICS:\t %d\n\tPRESENT:\t %d",
					qfi.graphicsFamily, qfi.presentFamily);
			break;
		}
		VkBool32 present_support = false;
		VkQueueFamilyProperties q = qfs[i];
		if (q.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			qfi.graphicsFamily = i;
		}
		if (idisp.vkGetPhysicalDeviceSurfaceSupportKHR(physdev, i, surf, &present_support) != VK_SUCCESS) {
			ENGINE_ERROR("Could not query for presentation support.");
			return ENGINE_FAILURE;
		}
		if (present_support) {
			qfi.presentFamily = i;
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

	std::set<int32_t> unique_qfis = {qfi.graphicsFamily, qfi.presentFamily};
	std::vector<VkDeviceQueueCreateInfo> qci_vec;
	float qp = 1.0f;
	for (int32_t q : unique_qfis) {
		VkDeviceQueueCreateInfo qci;
		qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		qci.pNext = NULL;
		qci.flags = {};
		qci.queueFamilyIndex = static_cast<uint32_t>(q);
		qci.queueCount = 1;
		qci.pQueuePriorities = &qp;
		qci_vec.push_back(qci);
	}

	VkDeviceCreateInfo ci;
	ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	ci.pNext = NULL;
	ci.flags = {};
	ci.queueCreateInfoCount = qci_vec.size();
	ci.pQueueCreateInfos = qci_vec.data();
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

EngineResult VulkanEngine::create_swapchain() {
	// Choose format from available formats
	VkSurfaceFormatKHR chosen_fmt;
	int chosen = 0;
	for (int i = 0; i < available_fmts.size(); i++) {
		VkSurfaceFormatKHR fmt = available_fmts.at(i);
		if (fmt.format == VK_FORMAT_B8G8R8A8_SRGB && 
			fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			chosen_fmt = fmt;
			chosen = 1;
		}
	}
	if (chosen == 0) {
		chosen_fmt = available_fmts.at(0);
	}

	// Choose presentation mode from available modes
	VkPresentModeKHR chosen_present_mode;
	chosen = 0;
	for (int i = 0; i < available_present_modes.size(); i++) {
		VkPresentModeKHR mode = available_present_modes.at(i);
		if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
			chosen_present_mode = mode;
			chosen = 1;
		}
	}
	if (chosen == 0) {
		chosen_present_mode = VK_PRESENT_MODE_FIFO_KHR;
	}

	// Fetch the swapchain extent
	swapchainExtent = surfcaps.currentExtent;
	ENGINE_MESSAGE_ARGS("Detected swapchain extent:\n\t%dw x %dh",
			swapchainExtent.width, swapchainExtent.height);

	queue_family_indices qfi;
	find_queue_families(physdev, &qfi);
	uint32_t uint_qfi[] = {
		static_cast<uint32_t>(qfi.graphicsFamily), 
		static_cast<uint32_t>(qfi.presentFamily)
	};

	VkSwapchainCreateInfoKHR ci;
	ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	ci.pNext = NULL;
	ci.flags = {};
	ci.surface = surf;
	ci.minImageCount = surfcaps.minImageCount + 1;
	ci.imageFormat = chosen_fmt.format;
	ci.imageColorSpace = chosen_fmt.colorSpace;
	ci.imageExtent = swapchainExtent;
	ci.imageArrayLayers = 1;
	ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	if (qfi.graphicsFamily == qfi.presentFamily) {
		ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		ci.queueFamilyIndexCount = 0;
		ci.pQueueFamilyIndices = NULL;
	} else {
		ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		ci.queueFamilyIndexCount = 2;
		ci.pQueueFamilyIndices = uint_qfi;
	}
	ci.preTransform = surfcaps.currentTransform;
	ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	ci.presentMode = chosen_present_mode;
	ci.clipped = VK_TRUE;
	ci.oldSwapchain = VK_NULL_HANDLE;

	if (ddisp.vkCreateSwapchainKHR(dev, &ci, NULL, &swapchain) != VK_SUCCESS) {
		ENGINE_ERROR("Could not create swapchain.");
		return ENGINE_FAILURE;
	}
	ENGINE_MESSAGE("Created swapchain.");
	return ENGINE_SUCCESS;
}
