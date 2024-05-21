#include "vk_engine.h"
#include "vk_images.h"

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTION 1
#include "vk_mem_alloc.h"


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

	// Initialise the memory allocator
	VmaVulkanFunctions vulkanFunctions = {};
	vulkanFunctions.vkGetInstanceProcAddr = instanceDispatch.vkGetInstanceProcAddr;
	vulkanFunctions.vkGetDeviceProcAddr = deviceDispatch.vkGetDeviceProcAddr;

	VmaAllocatorCreateInfo ai = {};
	ai.vulkanApiVersion = VK_API_VERSION_1_3;
	ai.physicalDevice = physicalDevice;
	ai.device = device;
	ai.instance = instance;
	ai.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	ai.pVulkanFunctions = &vulkanFunctions;
	vmaCreateAllocator(&ai, &allocator);

	mainDeletionQueue.push_function([&]() {
		vmaDestroyAllocator(allocator);
	});

	ENGINE_RUN_FN(create_swapchain);
	ENGINE_RUN_FN(init_commands);
	ENGINE_RUN_FN(init_sync);
	ENGINE_RUN_FN(init_descriptors);
	ENGINE_RUN_FN(init_pipelines);
	ENGINE_RUN_FN(init_imgui);

	return ENGINE_SUCCESS;
}

void VulkanEngine::deinit() {
	if (device != NULL) {
		deviceDispatch.vkDeviceWaitIdle(device);
		ENGINE_MESSAGE("Waiting for device to idle.");
	}
	mainDeletionQueue.flush();
	ENGINE_MESSAGE("Flushing main deletor queue.")

	for (int i = 0; i < FRAME_OVERLAP; i++) {
		if (frames[i].renderFence != NULL) {
			ENGINE_MESSAGE_ARGS("Destroying render fence %d", i);
			deviceDispatch.vkDestroyFence(device, frames[i].renderFence, NULL);
		}
		if (frames[i].renderSemaphore != NULL) {
			ENGINE_MESSAGE_ARGS("Destroying render semaphore %d", i);
			deviceDispatch.vkDestroySemaphore(device, frames[i].renderSemaphore, NULL);
		}
		if (frames[i].swapchainSemaphore != NULL) {
			ENGINE_MESSAGE_ARGS("Destroying swapchain semaphore %d", i);
			deviceDispatch.vkDestroySemaphore(device, frames[i].swapchainSemaphore, NULL);
		}
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
	ENGINE_MESSAGE("Loading libvulkan.so.1.");
	lib = dlopen("libvulkan.so.1", RTLD_NOW);
#endif

	if (lib == nullptr) {
		ENGINE_ERROR("Could not find Vulkan library. Please make sure you have a vulkan compatible graphics driver installed.")
		return ENGINE_FAILURE;
	}	

	PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)load_proc_addr(lib, "vkGetInstanceProcAddr");
	if (!vkGetInstanceProcAddr) {
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

	VkApplicationInfo ai = {};
	ai.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	ai.pNext = NULL;
	ai.pApplicationName = "VulkanEngine";
	ai.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
	ai.pEngineName = "Scotty's Engine";
	ai.engineVersion = VK_MAKE_VERSION(0, 1, 0);
	ai.apiVersion = VK_MAKE_API_VERSION(1, 3, 0, 0);

	VkInstanceCreateInfo ci = {};
	ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	ci.pNext = NULL;
	ci.flags = {};
	ci.pApplicationInfo = &ai;
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
	if (extensions == NULL) {
		return ENGINE_FAILURE;
	}
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

	return devprops.properties.deviceType == /*VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&*/
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

	VkPhysicalDeviceVulkan13Features feats13 = {};
	feats13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	feats13.dynamicRendering = VK_TRUE;
	feats13.synchronization2 = VK_TRUE;
	feats13.pNext = NULL;

	VkPhysicalDeviceVulkan12Features feats12 = {};
	feats12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	feats12.bufferDeviceAddress = VK_TRUE;
	feats12.descriptorIndexing = VK_TRUE;
	feats12.pNext = &feats13;

	VkDeviceCreateInfo ci = {};
	ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	ci.pNext = &feats12;
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
	ci.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
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

	VkExtent3D drawImageExtent = {
		1280,
		720,
		1
	};

	drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	drawImage.imageExtent = drawImageExtent;

	VkImageUsageFlags drawImageUsages{};
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	VkImageCreateInfo rImgInfo = {};
	rImgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	rImgInfo.pNext = nullptr;
	rImgInfo.imageType = VK_IMAGE_TYPE_2D;
	rImgInfo.format = drawImage.imageFormat;
	rImgInfo.extent = drawImage.imageExtent;
	rImgInfo.mipLevels = 1;
	rImgInfo.arrayLayers = 1;
	rImgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	rImgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	rImgInfo.usage = drawImageUsages;

	VmaAllocationCreateInfo rImgAllocInfo = {};
	rImgAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	rImgAllocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	vmaCreateImage(allocator, &rImgInfo, &rImgAllocInfo, &drawImage.image, &drawImage.allocation, nullptr);

	VkImageViewCreateInfo rViewInfo = {};
	rViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	rViewInfo.pNext = nullptr;
	rViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	rViewInfo.image = drawImage.image;
	rViewInfo.format = drawImage.imageFormat;
	rViewInfo.subresourceRange.baseMipLevel = 0;
	rViewInfo.subresourceRange.levelCount = 1;
	rViewInfo.subresourceRange.baseArrayLayer = 0;
	rViewInfo.subresourceRange.layerCount = 1;
	rViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

	if (deviceDispatch.vkCreateImageView(device, &rViewInfo, nullptr, &drawImage.imageView) != VK_SUCCESS) {
		ENGINE_ERROR("Could not create image view.");
		return ENGINE_FAILURE;
	}

	mainDeletionQueue.push_function([=]() {
		deviceDispatch.vkDestroyImageView(device, drawImage.imageView, nullptr);
		vmaDestroyImage(allocator, drawImage.image, drawImage.allocation);
	});

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
			ENGINE_ERROR("Failed to create command pool.");
			return ENGINE_FAILURE;
		}
		ENGINE_MESSAGE_ARGS("Created command pool %d", i);

		// allocate a single command buffer for rendering
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

	// immediate submits
	if (deviceDispatch.vkCreateCommandPool(device, &ci, NULL, &immCmdPool) != VK_SUCCESS) {
		ENGINE_ERROR("Failed to create immediate submit command pool.");
		return ENGINE_FAILURE;
	}
	ENGINE_MESSAGE_ARGS("Created immediate submit command pool.");

	VkCommandBufferAllocateInfo ai = {};
	ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	ai.pNext = NULL;
	ai.commandPool = immCmdPool;
	ai.commandBufferCount = 1;
	ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	if (deviceDispatch.vkAllocateCommandBuffers(device, &ai, &immCmdBuf) != VK_SUCCESS) {
		ENGINE_ERROR("Failed to allocate immediate submit command buffers.");
		return ENGINE_FAILURE;
	}
	mainDeletionQueue.push_function([=]() {
		deviceDispatch.vkDestroyCommandPool(device, immCmdPool, NULL);
	});

	return ENGINE_SUCCESS;
}

EngineResult VulkanEngine::init_sync() {
	// one fence to control when the gpu has finished rendering the frame,
	// and two sempaphores to sync rendering with the swapchain
	VkFenceCreateInfo fenceCi = {};
	fenceCi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCi.pNext = NULL;
	fenceCi.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VkSemaphoreCreateInfo semaphoreCi = {};
	semaphoreCi.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreCi.pNext = NULL;
	semaphoreCi.flags = 0;

	for (int i = 0; i < FRAME_OVERLAP; i++) {
		if (deviceDispatch.vkCreateFence(device, &fenceCi, NULL, &frames[i].renderFence) != VK_SUCCESS) {
			ENGINE_ERROR("Failed to create fence.");
			return ENGINE_FAILURE;
		}
		if (deviceDispatch.vkCreateSemaphore(device, &semaphoreCi, NULL, &frames[i].swapchainSemaphore) != VK_SUCCESS) {
			ENGINE_ERROR("Failed to create swapchain semaphore.");
			return ENGINE_FAILURE;
		}
		if (deviceDispatch.vkCreateSemaphore(device, &semaphoreCi, NULL, &frames[i].renderSemaphore) != VK_SUCCESS) {
			ENGINE_ERROR("Failed to create swapchain semaphore.");
			return ENGINE_FAILURE;
		}
	}

	if (deviceDispatch.vkCreateFence(device, &fenceCi, NULL, &immFence) != VK_SUCCESS) {
		ENGINE_ERROR("Failed to create immediate submit fence.");
		return ENGINE_FAILURE;
	}
	mainDeletionQueue.push_function([=]() {
		deviceDispatch.vkDestroyFence(device, immFence, NULL);
	});

	return ENGINE_SUCCESS;
}

EngineResult VulkanEngine::init_descriptors() {
	std::vector<DescriptorAllocator::PoolSizeRatio> sizes =
	{
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }
	};

	descriptorAllocator.init(device, 10, sizes, &deviceDispatch);

	{
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		drawImageDescriptorLayout = builder.build(device, VK_SHADER_STAGE_COMPUTE_BIT,
			&deviceDispatch);
	}

	// Allocate descriptor set for draw image
	drawImageDescriptors = descriptorAllocator.alloc(device, drawImageDescriptorLayout,
		&deviceDispatch);

	VkDescriptorImageInfo imgInfo = {};
	imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	imgInfo.imageView = drawImage.imageView;

	VkWriteDescriptorSet drawImageWrite = {};
	drawImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	drawImageWrite.pNext = NULL;
	drawImageWrite.dstBinding = 0;
	drawImageWrite.dstSet = drawImageDescriptors;
	drawImageWrite.descriptorCount = 1;
	drawImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	drawImageWrite.pImageInfo = &imgInfo;

	deviceDispatch.vkUpdateDescriptorSets(device, 1, &drawImageWrite, 0, nullptr);

	mainDeletionQueue.push_function([&] {
		deviceDispatch.vkDestroyDescriptorSetLayout(device, drawImageDescriptorLayout, NULL);
		descriptorAllocator.destroy_pool(device, &deviceDispatch);
	});

	return ENGINE_SUCCESS;
}

EngineResult VulkanEngine::init_pipelines() {
	if (init_background_pipelines() != ENGINE_SUCCESS) {
		return ENGINE_FAILURE;
	}
	return ENGINE_SUCCESS;
}

EngineResult VulkanEngine::init_background_pipelines() {
	VkPipelineLayoutCreateInfo ci = {};
	ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	ci.pNext = NULL;
	ci.pSetLayouts = &drawImageDescriptorLayout;
	ci.setLayoutCount = 1;

	VkPushConstantRange pushConst = {};
	pushConst.offset = 0;
	pushConst.size = sizeof(ComputePushConstants);
	pushConst.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	ci.pPushConstantRanges = &pushConst;
	ci.pushConstantRangeCount = 1;

	if (deviceDispatch.vkCreatePipelineLayout(device, &ci, NULL, &gradientPipelineLayout) != VK_SUCCESS) {
		ENGINE_ERROR("Failed to create pipeline layout.");
		return ENGINE_FAILURE;
	}

	VkShaderModule gradientShader;
	if (!load_shader_module("../../shaders/gradient_color.spv", device, &gradientShader,
		&deviceDispatch)) {
		ENGINE_ERROR("Failed to build the compute shader.");
		return ENGINE_FAILURE;
	}

	VkShaderModule skyShader;
	if (!load_shader_module("../../shaders/sky.spv", device, &skyShader,
		&deviceDispatch)) {
		ENGINE_ERROR("Failed to build the compute shader.");
		return ENGINE_FAILURE;
	}

	VkPipelineShaderStageCreateInfo si = {};
	si.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	si.pNext = NULL;
	si.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	si.module = gradientShader;
	si.pName = "main";

	VkComputePipelineCreateInfo computeCi = {};
	computeCi.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computeCi.pNext = NULL;
	computeCi.layout = gradientPipelineLayout;
	computeCi.stage = si;
	computeCi.basePipelineHandle = VK_NULL_HANDLE;

	ComputeEffect gradient;
	gradient.layout = gradientPipelineLayout;
	gradient.name = "gradient";
	gradient.data = {};
	gradient.data.data1 = glm::vec4(1, 0, 0, 1);
	gradient.data.data2 = glm::vec4(0, 0, 1, 1);

	VK_RUN_FN(deviceDispatch.vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computeCi, NULL,
		&gradient.pipeline),
		"Failed to create compute pipeline");

	computeCi.stage.module = skyShader;

	ComputeEffect sky;
	sky.layout = gradientPipelineLayout;
	sky.name = "sky";
	sky.data = {};
	sky.data.data1 = glm::vec4(0.1, 0.2, 0.4, 0.97);

	VK_RUN_FN(deviceDispatch.vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computeCi, NULL,
		&sky.pipeline),
		"Failed to create compute pipeline");

	backgroundEffects.push_back(gradient);
	backgroundEffects.push_back(sky);

	deviceDispatch.vkDestroyShaderModule(device, gradientShader, NULL);
	deviceDispatch.vkDestroyShaderModule(device, skyShader, NULL);

	mainDeletionQueue.push_function([&] {
		deviceDispatch.vkDestroyPipelineLayout(device, gradientPipelineLayout, NULL);
		deviceDispatch.vkDestroyPipeline(device, gradient.pipeline, NULL);
		deviceDispatch.vkDestroyPipeline(device, sky.pipeline, NULL);
	});

	return ENGINE_SUCCESS;
}

static PFN_vkVoidFunction imgui_function_loader(const char* function_name, void* user_data) {
	ImguiLoaderData* loaderData = (ImguiLoaderData*)user_data;
	VkInstance instance = (VkInstance)loaderData->instance;
	InstanceDispatch* instanceDispatch = (InstanceDispatch*)loaderData->instanceDispatch;

	return instanceDispatch->vkGetInstanceProcAddr(instance, function_name);
}

EngineResult VulkanEngine::init_imgui() {
	VkDescriptorPoolSize poolSizes[] = { { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 } };

	VkDescriptorPoolCreateInfo pi = {};
	pi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pi.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pi.maxSets = 1000;
	pi.poolSizeCount = (uint32_t)std::size(poolSizes);
	pi.pPoolSizes = poolSizes;

	VkDescriptorPool imguiPool;
	VK_RUN_FN(deviceDispatch.vkCreateDescriptorPool(device, &pi, NULL, &imguiPool),
		"Failed to create imgui descriptor pool.");

	ImGui::CreateContext();
	ImGui_ImplSDL2_InitForVulkan(window);

	ImGui_ImplVulkan_InitInfo initInfo = {};
	initInfo.Instance = instance;
	initInfo.PhysicalDevice = physicalDevice;
	initInfo.Device = device;
	initInfo.Queue = graphicsQueue;
	initInfo.DescriptorPool = imguiPool;
	initInfo.MinImageCount = 3;
	initInfo.ImageCount = 3;
	initInfo.UseDynamicRendering = true;
	initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineRenderingCreateInfo ci = {};
	ci.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	ci.pNext = NULL;
	ci.viewMask = 0;
	ci.colorAttachmentCount = 1;
	ci.pColorAttachmentFormats = &swapchainFormat;
	ci.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
	ci.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

	initInfo.PipelineRenderingCreateInfo = ci;
	
	ImguiLoaderData loaderData = { .instance = instance, .instanceDispatch = &instanceDispatch };
	ImGui_ImplVulkan_LoadFunctions(imgui_function_loader, (void*)&loaderData);
	ImGui_ImplVulkan_Init(&initInfo);


	ImGui_ImplVulkan_CreateFontsTexture();

	mainDeletionQueue.push_function([=]() {
		deviceDispatch.vkDestroyDescriptorPool(device, imguiPool, NULL);
		ImGui_ImplVulkan_Shutdown();
	});

	return ENGINE_SUCCESS;
}

EngineResult VulkanEngine::draw() {
	if (deviceDispatch.vkWaitForFences(device, 1, &get_current_frame().renderFence, true, TIMEOUT_N) != VK_SUCCESS) {
		ENGINE_ERROR("Failed to wait for fences.");
		return ENGINE_FAILURE;
	}
	get_current_frame().deletionQueue.flush();
	if (deviceDispatch.vkResetFences(device, 1, &get_current_frame().renderFence) != VK_SUCCESS) {
		ENGINE_ERROR("Failed to reset fences.");
		return ENGINE_FAILURE;
	}

	uint32_t swapchainImgIndex;
	if (deviceDispatch.vkAcquireNextImageKHR(device, swapchain, TIMEOUT_N, get_current_frame().swapchainSemaphore,
		NULL, &swapchainImgIndex) != VK_SUCCESS) {
		ENGINE_ERROR("Failed to aquire next image from swapchain.");
		return ENGINE_FAILURE;
	}

	VkCommandBuffer cmd = get_current_frame().cmdBuf;
	if (deviceDispatch.vkResetCommandBuffer(cmd, 0) != VK_SUCCESS) {
		ENGINE_ERROR("Failed to reset command buffer.");
		return ENGINE_FAILURE;
	}

//> Draw first
	drawExtent.width = drawImage.imageExtent.width;
	drawExtent.height = drawImage.imageExtent.height;

	VkCommandBufferBeginInfo cmdBi = {};
	cmdBi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBi.pNext = NULL;
	cmdBi.pInheritanceInfo = NULL;
	cmdBi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	if (deviceDispatch.vkBeginCommandBuffer(cmd, &cmdBi) != VK_SUCCESS) {
		ENGINE_ERROR("Failed to begin command buffer.");
		return ENGINE_FAILURE;
	}

	// Transition main draw image into general layout so we can write into it
	transition_image(cmd, drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		&deviceDispatch);

	draw_background(cmd);

	// Transition the draw image and the swapchain image into their correct trnasfer layouts
	transition_image(cmd, drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		&deviceDispatch);
	transition_image(cmd, swapchainImgs[swapchainImgIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		&deviceDispatch);
//< Draw first
//> Draw imgui
	copy_image_to_image(cmd, drawImage.image, swapchainImgs[swapchainImgIndex], drawExtent, swapchainExtent,
		&deviceDispatch);

	transition_image(cmd, swapchainImgs[swapchainImgIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, &deviceDispatch);

	draw_imgui(cmd, swapchainImgViews[swapchainImgIndex]);

	transition_image(cmd, swapchainImgs[swapchainImgIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		&deviceDispatch);
//< Draw imgui

	if (deviceDispatch.vkEndCommandBuffer(cmd) != VK_SUCCESS) {
		ENGINE_ERROR("Failed to end recording to command buffer.");
		return ENGINE_FAILURE;
	}

	VkCommandBufferSubmitInfo cmdSi = {};
	cmdSi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	cmdSi.pNext = NULL;
	cmdSi.commandBuffer = cmd;
	cmdSi.deviceMask = 0;

	VkSemaphoreSubmitInfo semWi = {};
	semWi.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	semWi.pNext = NULL;
	semWi.semaphore = get_current_frame().swapchainSemaphore;
	semWi.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;
	semWi.deviceIndex = 0;
	semWi.value = 1;

	VkSemaphoreSubmitInfo semSi = {};
	semSi.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	semSi.pNext = NULL;
	semSi.semaphore = get_current_frame().renderSemaphore;
	semSi.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
	semSi.deviceIndex = 0;
	semSi.value = 1;

	VkSubmitInfo2 submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submitInfo.pNext = NULL;
	submitInfo.waitSemaphoreInfoCount = 1;
	submitInfo.pWaitSemaphoreInfos = &semWi;
	submitInfo.signalSemaphoreInfoCount = 1;
	submitInfo.pSignalSemaphoreInfos = &semSi;
	submitInfo.commandBufferInfoCount = 1;
	submitInfo.pCommandBufferInfos = &cmdSi;

	if (deviceDispatch.vkQueueSubmit2(graphicsQueue, 1, &submitInfo, get_current_frame().renderFence) != VK_SUCCESS) {
		ENGINE_ERROR("Could not submit command buffer.");
		return ENGINE_FAILURE;
	}

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = NULL;
	presentInfo.pSwapchains = &swapchain;
	presentInfo.swapchainCount = 1;

	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &get_current_frame().renderSemaphore;

	presentInfo.pImageIndices = &swapchainImgIndex;

	if (deviceDispatch.vkQueuePresentKHR(graphicsQueue, &presentInfo) != VK_SUCCESS) {
		ENGINE_ERROR("Could not present.");
		return ENGINE_FAILURE;
	}

	frameNumber++;
	return ENGINE_SUCCESS;
}

void VulkanEngine::draw_background(VkCommandBuffer cmd) {
	VkClearColorValue clearValue;
	float flash = abs(sin(frameNumber / 120.f));
	clearValue = { {0.0f, 0.0f, flash, 1.0f} };

	VkImageSubresourceRange clearRange = {};
	clearRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	clearRange.baseMipLevel = 0;
	clearRange.levelCount = VK_REMAINING_MIP_LEVELS;
	clearRange.baseArrayLayer = 0;
	clearRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

	ComputeEffect& effect = backgroundEffects[currentBackgroundEffect];

	deviceDispatch.vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);
	deviceDispatch.vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, gradientPipelineLayout,
		0, 1, &drawImageDescriptors, 0, NULL);

	deviceDispatch.vkCmdPushConstants(cmd, gradientPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
		sizeof(ComputePushConstants), &effect.data);

	deviceDispatch.vkCmdDispatch(cmd, std::ceil(drawExtent.width / 16.0), std::ceil(drawExtent.height / 16.0), 1);
}


EngineResult VulkanEngine::immediate_submit(std::function<void(VkCommandBuffer cmd)>&& fn) {
	if (deviceDispatch.vkResetFences(device, 1, &immFence) != VK_SUCCESS) {
		ENGINE_ERROR("Failed to reset immedaite submit fences.");
		return ENGINE_FAILURE;
	}
	if (deviceDispatch.vkResetCommandBuffer(immCmdBuf, 0) != VK_SUCCESS) {
		ENGINE_ERROR("Failed to reset immedaite submit command buffer.");
		return ENGINE_FAILURE;
	}

	VkCommandBuffer cmd = immCmdBuf;

	VkCommandBufferBeginInfo cmdBi = {};
	cmdBi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBi.pNext = NULL;
	cmdBi.pInheritanceInfo = NULL;
	cmdBi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VK_RUN_FN(deviceDispatch.vkBeginCommandBuffer(cmd, &cmdBi),
		"Failed to begin command buffer.");

	fn(cmd);

	VK_RUN_FN(deviceDispatch.vkEndCommandBuffer(cmd),
		"Failed to end command buffer.");

	VkCommandBufferSubmitInfo cmdSi = {};
	cmdSi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	cmdSi.pNext = NULL;
	cmdSi.commandBuffer = cmd;
	cmdSi.deviceMask = 0;

	VkSubmitInfo2 submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submitInfo.pNext = NULL;
	submitInfo.waitSemaphoreInfoCount = 0;
	submitInfo.pWaitSemaphoreInfos = NULL;
	submitInfo.signalSemaphoreInfoCount = 0;
	submitInfo.pSignalSemaphoreInfos = NULL;
	submitInfo.commandBufferInfoCount = 1;
	submitInfo.pCommandBufferInfos = &cmdSi;

	VK_RUN_FN(deviceDispatch.vkQueueSubmit2(graphicsQueue, 1, &submitInfo, immFence),
		"Failed to submit command.");

	VK_RUN_FN(deviceDispatch.vkWaitForFences(device, 1, &immFence, true, 9999999999),
		"Failed to wait for fences");
}

EngineResult VulkanEngine::draw_imgui(VkCommandBuffer cmd, VkImageView targetImgView) {
	VkRenderingAttachmentInfo rAttachInfo = {};
	rAttachInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	rAttachInfo.pNext = NULL;
	rAttachInfo.imageView = targetImgView;
	rAttachInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	rAttachInfo.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	rAttachInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	VkRenderingInfo rInfo = {};
	rInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	rInfo.pNext = NULL;
	rInfo.renderArea = VkRect2D{ VkOffset2D {0, 0}, swapchainExtent };
	rInfo.layerCount = 1;
	rInfo.colorAttachmentCount = 1;
	rInfo.pColorAttachments = &rAttachInfo;
	rInfo.pDepthAttachment = NULL;
	rInfo.pStencilAttachment = NULL;

	deviceDispatch.vkCmdBeginRendering(cmd, &rInfo);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

	deviceDispatch.vkCmdEndRendering(cmd);

	return ENGINE_SUCCESS;
}
