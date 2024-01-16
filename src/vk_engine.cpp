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
	ENGINE_RUN_FN(init_commands);

	return ENGINE_SUCCESS;
}

void VulkanEngine::deinit() {
	for (int i = 0; i < FRAME_OVERLAP; i++) {
		if (frames[i].cmdPool != NULL) {
			ENGINE_MESSAGE_ARGS("Destroying command pool %d", i);
			deviceDispatch.vkDestroyCommandPool(device, frames[i].cmdPool, NULL);
		}
	}

	if (swapchain != NULL) {
		ENGINE_MESSAGE("Destroying swapchain image views.");
		for (int i = 0; i < swapchainImgViews.size(); i++) {
			deviceDispatch.vkDestroyImageView(device, swapchainImgViews[i], NULL);
		}
		ENGINE_MESSAGE("Destroying swapchain.");
		deviceDispatch.vkDestroySwapchainKHR(device, swapchain, NULL);
	}

	if (device != NULL) {
		ENGINE_MESSAGE("Destroying logical device.");
		deviceDispatch.vkDestroyDevice(device, NULL);
	}

	if (surface != NULL) {
		ENGINE_MESSAGE("Destroying surface.");
		instanceDispatch.vkDestroySurfaceKHR(instance, surface, NULL);
	}

	if (instance != NULL) {
		ENGINE_MESSAGE("Destroying instance.");
		instanceDispatch.vkDestroyInstance(instance, NULL);
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
	if (!instanceDispatch.vkGetInstanceProcAddr) {
		ENGINE_ERROR("Could not load vkGetInstanceProcAddr.");
		return ENGINE_FAILURE;
	}

	ENGINE_MESSAGE("Successfully loaded the Vulkan loader.");

	// First time calling this function for global level instance functions
	load_instance_dispatch_table(&instanceDispatch, vkGetInstanceProcAddr, NULL);

	return ENGINE_SUCCESS;
}

EngineResult VulkanEngine::create_instance() {
	unsigned int sdlExtensionCount = 0;
	SDL_Vulkan_GetInstanceExtensions(window, &sdlExtensionCount, NULL);
	std::vector<const char*> extensions(sdlExtensionCount);
	if (SDL_Vulkan_GetInstanceExtensions(window, &sdlExtensionCount, extensions.data()) != SDL_TRUE) {
		return ENGINE_FAILURE;
	}

	// This is commented out because SDL will automatically get VK_KHR_surface...
	// uncomment if need extra extensions that SDL doesn't fetch automatically
	/*for (int i = 0; i < vkInstanceExtensions.size(); i++) {
		extensions.push_back(vkInstanceExtensions.at(i));
	}*/

	ENGINE_MESSAGE("Attempting to create instance with the following extensions:");
	for (int i = 0; i < extensions.size(); i++) {
		fprintf(stderr, "\t%s\n", extensions.at(i));
	}	

	VkInstanceCreateInfo ci = {};
	ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	ci.pNext = NULL;
	ci.flags = {};
	ci.pApplicationInfo = NULL;
	ci.enabledLayerCount = 0;
	ci.ppEnabledLayerNames = NULL;
	ci.enabledExtensionCount = extensions.size();
	ci.ppEnabledExtensionNames = extensions.data();

	if (instanceDispatch.vkCreateInstance(&ci, NULL, &instance) != VK_SUCCESS) {
		ENGINE_ERROR("Unable to create Vulkan instance.");
		return ENGINE_FAILURE;
	}

	// Second time calling this function for dispatch level instance functions
	load_instance_dispatch_table(&instanceDispatch, NULL, instance);

	return ENGINE_SUCCESS;
}

uint32_t VulkanEngine::device_suitable(VkPhysicalDevice device) {
	availableFormats.clear();
	availablePresentModes.clear();

	VkPhysicalDeviceProperties2 devprops = {};
	devprops.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	instanceDispatch.vkGetPhysicalDeviceProperties2(device, &devprops);
	VkPhysicalDeviceFeatures devfeats = {};
	instanceDispatch.vkGetPhysicalDeviceFeatures(device, &devfeats);

	ENGINE_MESSAGE_ARGS("Found device %s: ", devprops.properties.deviceName);

	// Query extension Support
	uint32_t extensionCount;
	instanceDispatch.vkEnumerateDeviceExtensionProperties(device, NULL, &extensionCount, NULL);
	if (extensionCount == 0) {
		ENGINE_WARNING("No extension support detected.");
		return 0;
	}
	VkExtensionProperties *extensions = (VkExtensionProperties *)malloc(extensionCount * sizeof(VkExtensionProperties));
	if (instanceDispatch.vkEnumerateDeviceExtensionProperties(device, NULL, &extensionCount, extensions) != VK_SUCCESS) {
		ENGINE_WARNING("Failed to fetch extension properties.");
		free(extensions);
		return 0;
	}

	for (int i = 0; i < vkDeviceExtensions.size(); i++) {
		uint32_t extensionFound = 0;
		for (int j = 0; i < extensionCount; j++) {
			if (!strcmp(vkDeviceExtensions[i], extensions[j].extensionName)) {
				extensionFound = 1;
				break;
			}
		}
		if (!extensionFound) {
			ENGINE_WARNING("Device does not support requested extensions.");
			free(extensions);
			return 0;
		}
	}
	free(extensions);

	// Start surface formats support
	uint32_t formatsCount = 0;
	instanceDispatch.vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatsCount, NULL);
	if (formatsCount == 0) {
		ENGINE_WARNING("No surface formats detected.");
		return 0;
	}
	availableFormats.resize(formatsCount);
	if (instanceDispatch.vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatsCount, availableFormats.data())
		!= VK_SUCCESS) {
		ENGINE_WARNING("Failed to fetch physical device surface formats.");
		return 0;
	}

	// Start surface present modes support
	uint32_t present_modes_count = 0;
	instanceDispatch.vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_modes_count, NULL);
	if (present_modes_count == 0) {
		ENGINE_WARNING("No surface present modes detected.");
		return 0;
	}
	availablePresentModes.resize(present_modes_count);
	if (instanceDispatch.vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_modes_count, availablePresentModes.data())
		!= VK_SUCCESS) {
		ENGINE_WARNING("Failed to fetch physical device surface present modes.");
		return 0;
	}

	// Start surface capabilities
	if (instanceDispatch.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &surfaceCaps) != VK_SUCCESS) {
		ENGINE_WARNING("Could not query for physical device surface capabilities.\n");
		return 0;
	}

	return devprops.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
		devfeats.geometryShader;
}

EngineResult VulkanEngine::select_physical_device() {
	ENGINE_MESSAGE("Finding a suitable physical device...");
	uint32_t deviceCount;
	instanceDispatch.vkEnumeratePhysicalDevices(instance, &deviceCount, NULL);

	if (deviceCount == 0) {
		ENGINE_ERROR("Could not find any devices.");
		return ENGINE_FAILURE;
	}

	VkPhysicalDevice *devs = (VkPhysicalDevice*)malloc(deviceCount * sizeof(VkPhysicalDevice));
	if (instanceDispatch.vkEnumeratePhysicalDevices(instance, &deviceCount, devs) != VK_SUCCESS) {
		ENGINE_ERROR("Failed to enumerate physical devices.");
		free(devs);
		return ENGINE_FAILURE;
	}

	// Finds first suitable device which may not be the best
	for (int i = 0; i < deviceCount; i++) {
		if (device_suitable(devs[i])) {
			physicalDevice = devs[i];
			break;
		}
	}
	free(devs);

	if (physicalDevice == NULL) {
		ENGINE_ERROR("Failed to find a suitable GPU.");
		return ENGINE_FAILURE;
	}

	return ENGINE_SUCCESS;
}

static int queue_families_complete(QueueFamilyIndices *qfi) {
	return qfi->graphicsFamily > -1 && qfi->presentFamily > -1;
}

EngineResult VulkanEngine::find_queue_families(VkPhysicalDevice physicalDevice, QueueFamilyIndices *pQfi) {
	QueueFamilyIndices qfi;
	uint32_t qfc = 0;
	
	instanceDispatch.vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &qfc, NULL);
	if (qfc == 0) {
		ENGINE_ERROR("Could not detect any queue families for device.");
		return ENGINE_FAILURE;
	}
	VkQueueFamilyProperties *qfs = (VkQueueFamilyProperties*)malloc(qfc * sizeof(VkQueueFamilyProperties));
	instanceDispatch.vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &qfc, qfs);

	for (int i = 0; i < qfc; i++) {
		if (queue_families_complete(&qfi)) {
			ENGINE_MESSAGE_ARGS("Found queue families:\n\tNAME\t\tINDEX\n\tGRAPHICS:\t %d\n\tPRESENT:\t %d",
					qfi.graphicsFamily, qfi.presentFamily);
			break;
		}
		VkBool32 presentSupport = false;
		VkQueueFamilyProperties q = qfs[i];
		if (q.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			qfi.graphicsFamily = i;
		}
		if (instanceDispatch.vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport) != VK_SUCCESS) {
			ENGINE_ERROR("Could not query for presentation support.");
			return ENGINE_FAILURE;
		}
		if (presentSupport) {
			qfi.presentFamily = i;
		}
	}
	free (qfs);

	if (qfi.graphicsFamily == -1) {
		ENGINE_ERROR("Could not find graphics queue family.");
		return ENGINE_FAILURE;
		
	}

	memcpy(pQfi, &qfi, sizeof(QueueFamilyIndices));
	return ENGINE_SUCCESS;
}

EngineResult VulkanEngine::create_device() {
	if (find_queue_families(physicalDevice, &queueFamilies) != ENGINE_SUCCESS) {
		return ENGINE_FAILURE;
	}

	std::set<int32_t> uniqueIndices = {queueFamilies.graphicsFamily, queueFamilies.presentFamily};
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	float qp = 1.0f;
	for (int32_t q : uniqueIndices) {
		VkDeviceQueueCreateInfo qci = {};
		qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		qci.pNext = NULL;
		qci.flags = {};
		qci.queueFamilyIndex = static_cast<uint32_t>(q);
		qci.queueCount = 1;
		qci.pQueuePriorities = &qp;
		queueCreateInfos.push_back(qci);
	}

	VkDeviceCreateInfo ci = {};
	ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	ci.pNext = NULL;
	ci.flags = {};
	ci.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	ci.pQueueCreateInfos = queueCreateInfos.data();
	ci.enabledLayerCount = 0;
	ci.enabledExtensionCount = vkDeviceExtensions.size();
	ci.ppEnabledExtensionNames = vkDeviceExtensions.data();
	ci.pEnabledFeatures = NULL;

	if (instanceDispatch.vkCreateDevice(physicalDevice, &ci, NULL, &device) != VK_SUCCESS) {
		fprintf(stderr, "[VulkanEngine] Could not create logical device.");
		ENGINE_ERROR("Could not create logical device.");
		return ENGINE_FAILURE;
	};
	
	load_device_dispatch_table(&deviceDispatch, instanceDispatch.vkGetInstanceProcAddr, instance, device);
	ENGINE_MESSAGE("Created device and loaded device dispatch table.");

	deviceDispatch.vkGetDeviceQueue(device, queueFamilies.graphicsFamily, 0, &graphicsQueue);
	deviceDispatch.vkGetDeviceQueue(device, queueFamilies.presentFamily, 0, &graphicsQueue);

	return ENGINE_SUCCESS;
}

EngineResult VulkanEngine::create_surface() {
	if (SDL_Vulkan_CreateSurface(window, instance, &surface) != SDL_TRUE) {
		ENGINE_ERROR("Unable to create Vulkan surface from SDL window.");
		return ENGINE_FAILURE;
	}
	return ENGINE_SUCCESS;
}

EngineResult VulkanEngine::create_swapchain() {
	// Choose format from available formats
	VkSurfaceFormatKHR chosenFormat;
	int chosen = 0;
	for (int i = 0; i < availableFormats.size(); i++) {
		VkSurfaceFormatKHR surfaceFormat = availableFormats.at(i);
		if (surfaceFormat.format == VK_FORMAT_B8G8R8A8_SRGB && 
			surfaceFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			chosenFormat = surfaceFormat;
			chosen = 1;
		}
	}
	if (chosen == 0) {
		chosenFormat = availableFormats.at(0);
	}
	swapchainFormat = chosenFormat.format;

	// Choose presentation mode from available modes
	VkPresentModeKHR chosenPresentMode;
	chosen = 0;
	for (int i = 0; i < availablePresentModes.size(); i++) {
		VkPresentModeKHR mode = availablePresentModes.at(i);
		if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
			chosenPresentMode = mode;
			chosen = 1;
		}
	}
	if (chosen == 0) {
		chosenPresentMode = VK_PRESENT_MODE_FIFO_KHR;
	}

	// Fetch the swapchain extent
	swapchainExtent = surfaceCaps.currentExtent;
	ENGINE_MESSAGE_ARGS("Detected swapchain extent:\n\t%dw x %dh",
			swapchainExtent.width, swapchainExtent.height);

	uint32_t uintIndices[] = {
		static_cast<uint32_t>(queueFamilies.graphicsFamily), 
		static_cast<uint32_t>(queueFamilies.presentFamily)
	};

	uint32_t imgCount = surfaceCaps.minImageCount + 1;

	VkSwapchainCreateInfoKHR ci = {};
	ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	ci.pNext = NULL;
	ci.flags = {};
	ci.surface = surface;
	ci.minImageCount = imgCount;
	ci.imageFormat = swapchainFormat;
	ci.imageColorSpace = chosenFormat.colorSpace;
	ci.imageExtent = swapchainExtent;
	ci.imageArrayLayers = 1;
	ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	if (queueFamilies.graphicsFamily == queueFamilies.presentFamily) {
		ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		ci.queueFamilyIndexCount = 0;
		ci.pQueueFamilyIndices = NULL;
	} else {
		ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		ci.queueFamilyIndexCount = 2;
		ci.pQueueFamilyIndices = uintIndices;
	}
	ci.preTransform = surfaceCaps.currentTransform;
	ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	ci.presentMode = chosenPresentMode;
	ci.clipped = VK_TRUE;
	ci.oldSwapchain = VK_NULL_HANDLE;

	if (deviceDispatch.vkCreateSwapchainKHR(device, &ci, NULL, &swapchain) != VK_SUCCESS) {
		ENGINE_ERROR("Could not create swapchain.");
		return ENGINE_FAILURE;
	}
	ENGINE_MESSAGE("Created swapchain.");

	// Create images
	deviceDispatch.vkGetSwapchainImagesKHR(device, swapchain, &imgCount, NULL);
	swapchainImgs.resize(imgCount);
	if (deviceDispatch.vkGetSwapchainImagesKHR(device, swapchain, &imgCount, swapchainImgs.data()) != VK_SUCCESS) {
		ENGINE_ERROR("Could not retrieve swapchain images.");
		return ENGINE_FAILURE;
	}

	// Create image views
	swapchainImgViews.resize(swapchainImgs.size());
	for (int i = 0; i < swapchainImgs.size(); i ++) {
		VkImageViewCreateInfo imgViewCi = {};
		imgViewCi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imgViewCi.pNext = NULL;
		imgViewCi.flags = {};
		imgViewCi.image = swapchainImgs[i];
		imgViewCi.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imgViewCi.format = swapchainFormat;
		imgViewCi.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		imgViewCi.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		imgViewCi.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		imgViewCi.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		imgViewCi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imgViewCi.subresourceRange.baseMipLevel = 0;
		imgViewCi.subresourceRange.levelCount = 1;
		imgViewCi.subresourceRange.baseArrayLayer = 0;
		imgViewCi.subresourceRange.layerCount = 1;
		if (deviceDispatch.vkCreateImageView(device, &imgViewCi, NULL, &swapchainImgViews[i]) != VK_SUCCESS) {
			ENGINE_ERROR("Could not create swapchain image view.");
			return ENGINE_FAILURE;
		}
	}

	return ENGINE_SUCCESS;
}

EngineResult VulkanEngine::init_commands() {
	VkCommandPoolCreateInfo ci = {};
	ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	ci.pNext = NULL;
	ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	ci.queueFamilyIndex = queueFamilies.graphicsFamily;

	for (int i = 0; i < FRAME_OVERLAP; i++) {
		if (deviceDispatch.vkCreateCommandPool(device, &ci, NULL, &frames[i].cmdPool) != VK_SUCCESS) {
			ENGINE_ERROR("Could not create command pool.");
			return ENGINE_FAILURE;
		}
		ENGINE_MESSAGE_ARGS("Created command pool %d", i);

		// Allocate a single command buffer for rendering
		VkCommandBufferAllocateInfo ai = {};
		ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		ai.pNext = NULL;
		ai.commandPool = frames[i].cmdPool;
		ai.commandBufferCount = 1;
		ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		if (deviceDispatch.vkAllocateCommandBuffers(device, &ai, &frames[i].cmdBuf) != VK_SUCCESS) {
			ENGINE_ERROR("Could not allocate command buffer.");
			return ENGINE_FAILURE;
		}
	}
	return ENGINE_SUCCESS;
}
