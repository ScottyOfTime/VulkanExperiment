#include "vk_engine.h"

#include "vk_images.h"

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTION 1
#include <vk_mem_alloc.h>

#include <glslang/Public/ShaderLang.h>

#include <stb_image.h>

#if defined(VK_USE_PLATFORM_WIN32_KHR)
	#define load_proc_addr GetProcAddress
#elif defined(VK_USE_PLATFORM_XCB_KHR)
	#define load_proc_addr dlsym
#endif

/*  ============================================================================
 *  INIT FUNCTIONS
 *  ============================================================================
 *
 *  These are all functions that are involved in or related
 *  to initializing the VulkanEngine class.
 */

EngineResult 
VulkanEngine::init() {
	// Create SDL window
	if (SDL_Init(SDL_INIT_VIDEO) != SDL_TRUE) {
		ENGINE_ERROR("Could not initialize SDL");
		fprintf(stderr, "\tSDL Error: %s\n", SDL_GetError());
		return ENGINE_FAILURE;
	}
	window = SDL_CreateWindow("Game", 1280, 720, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
	if (window == NULL) {
		ENGINE_ERROR("Could not create SDL Window");
		fprintf(stderr, "\tSDL Error: %s\n", SDL_GetError());
	}

	ENGINE_RUN_FN(link());
	ENGINE_RUN_FN(create_instance());
	ENGINE_RUN_FN(create_surface());
	ENGINE_RUN_FN(select_physical_device());
	ENGINE_RUN_FN(create_device());

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

	mainDeletionQueue.push_function("vmaDestroyAllocator", [&]() {
		fprintf(stderr, "[VMA] Buffers created %d, buffers destroyed %d\n", buffersCreated,
			buffersDestroyed);
		vmaDestroyAllocator(allocator);
	});

	ENGINE_RUN_FN(init_swapchain());
	ENGINE_RUN_FN(init_commands());
	ENGINE_RUN_FN(init_sync());
	ENGINE_RUN_FN(init_descriptors());

	ENGINE_MESSAGE("Initializing GLSL");
	glslang::InitializeProcess();

	ENGINE_RUN_FN(init_pipelines());
	ENGINE_RUN_FN(init_imgui());

	ENGINE_RUN_FN(init_buffers());

	if (init_default_data() != ENGINE_SUCCESS) {
		return ENGINE_FAILURE;
	}

	// Begin the shader monitor thread (passing 'this' seems suspect)
	shaderMonitorThread = std::thread(&VulkanEngine::shader_monitor_thread, this);

	return ENGINE_SUCCESS;
}

void
VulkanEngine::deinit() {
	shutdown = 1;

	if (device != NULL) {
		deviceDispatch.vkDeviceWaitIdle(device);
		ENGINE_MESSAGE("Waiting for device to idle.");
	}

	if (shaderMonitorThread.joinable()) {
		shaderMonitorThread.join();
	}

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

		frames[i].deletionQueue.flush();
	}

	ENGINE_MESSAGE("Finalizing GLSL");
	
	glslang::FinalizeProcess();

	// Destroy all the shader modules
	for (size_t i = 0; i < shaderCount; i++) {
		deviceDispatch.vkDestroyShaderModule(device, shaders[i].shader,
			nullptr);
	}

	// Destroy all pipelines
	// Hack because the destroy_pipeline function decrements pipelineCount
	uint32_t numPipelines = pipelineCount;
	while (pipelineCount > 0) {
		destroy_pipeline(0);
	}

	mainDeletionQueue.flush();

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
	SDL_DestroyWindow(window);
	}
}

EngineResult
VulkanEngine::link() {
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

EngineResult
VulkanEngine::create_instance() {
	unsigned int sdlExtensionCount = 0;
	const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);
	/*std::vector<const char*> extensions(sdlExtensionCount);
	if (SDL_Vulkan_GetInstanceExtensions(window, &sdlExtensionCount, extensions.data()) != SDL_TRUE) {
		return ENGINE_FAILURE;
	}*/

	// This is commented out because SDL will automatically get VK_KHR_surface...
	// uncomment if need extra extensions that SDL doesn't fetch automatically
	/*for (int i = 0; i < vkInstanceExtensions.size(); i++) {
		extensions.push_back(vkInstanceExtensions.at(i));
	}*/

	ENGINE_MESSAGE("Attempting to create instance with the following extensions:");
	for (int i = 0; i < sdlExtensionCount; i++) {
		fprintf(stderr, "\t%s\n", extensions[i]);
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
	ci.enabledExtensionCount = sdlExtensionCount;
	ci.ppEnabledExtensionNames = extensions;

	if (instanceDispatch.vkCreateInstance(&ci, NULL, &instance) != VK_SUCCESS) {
		ENGINE_ERROR("Unable to create Vulkan instance.");
		return ENGINE_FAILURE;
	}

	// Second time calling this function for dispatch level instance functions
	load_instance_dispatch_table(&instanceDispatch, NULL, instance);

	return ENGINE_SUCCESS;
}

uint32_t
VulkanEngine::device_suitable(VkPhysicalDevice device) {
	availableFormats.clear();
	availablePresentModes.clear();

	// Query physical device features and propertes
	VkPhysicalDeviceProperties2 devprops = {};
	devprops.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	instanceDispatch.vkGetPhysicalDeviceProperties2(device, &devprops);

	// We want to use bindless descriptor indexing so make sure we have
	// support for that (check the return statement of this function
	// for particular features that are required)
	VkPhysicalDeviceDescriptorIndexingFeatures indexingfeats = {};
	indexingfeats.sType = 
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
	indexingfeats.pNext = nullptr;


	VkPhysicalDeviceFeatures2 devfeats = {};
	devfeats.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	devfeats.pNext = &indexingfeats;
	instanceDispatch.vkGetPhysicalDeviceFeatures2(device, &devfeats);
	

	ENGINE_MESSAGE_ARGS("Found device %s: ", devprops.properties.deviceName);
	printf("Max UBO size: %d\n", devprops.properties.limits.maxUniformBufferRange);

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
			// @TODO ->	SO BAD >:-(
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

	return devprops.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &
		devfeats.features.geometryShader &
		devfeats.features.wideLines &
		devfeats.features.fillModeNonSolid &
		indexingfeats.shaderSampledImageArrayNonUniformIndexing &
		indexingfeats.descriptorBindingSampledImageUpdateAfterBind &
		indexingfeats.shaderUniformBufferArrayNonUniformIndexing &
		indexingfeats.descriptorBindingUniformBufferUpdateAfterBind &
		indexingfeats.shaderStorageBufferArrayNonUniformIndexing &
		indexingfeats.descriptorBindingStorageBufferUpdateAfterBind;
}

EngineResult
VulkanEngine::select_physical_device() {
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
	// This check was implemented to shut VS up about a null pointer in the next lines of code
	if (devs == NULL) {
		ENGINE_ERROR("Failed to dereference device array.");
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

static int
queue_families_complete(QueueFamilyIndices *qfi) {
	return qfi->graphicsFamily > -1 && qfi->presentFamily > -1;
}

EngineResult
VulkanEngine::find_queue_families(VkPhysicalDevice physicalDevice, QueueFamilyIndices *pQfi) {
	QueueFamilyIndices qfi;
	uint32_t qfc = 0;
	
	instanceDispatch.vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &qfc, NULL);
	if (qfc == 0) {
		ENGINE_ERROR("Could not detect any queue families for device.");
		return ENGINE_FAILURE;
	}
	VkQueueFamilyProperties *qfs = (VkQueueFamilyProperties*)malloc(qfc * sizeof(VkQueueFamilyProperties));
	instanceDispatch.vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &qfc, qfs);
	if (qfs == nullptr) {
		ENGINE_ERROR("Failed to retrieve queue family properties");
		return ENGINE_FAILURE;
	}

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

EngineResult
VulkanEngine::create_device() {
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
	feats13.pNext = nullptr;

	VkPhysicalDeviceVulkan12Features feats12 = {};
	feats12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	feats12.bufferDeviceAddress = VK_TRUE;
	feats12.descriptorIndexing = VK_TRUE;
	feats12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
	feats12.descriptorBindingPartiallyBound = VK_TRUE;
	feats12.pNext = &feats13;

	VkPhysicalDeviceFeatures feats10 = {};
	feats10.wideLines = VK_TRUE;
	feats10.fillModeNonSolid = VK_TRUE;

	VkDeviceCreateInfo ci = {};
	ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	ci.pNext = &feats12;
	ci.flags = {};
	ci.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	ci.pQueueCreateInfos = queueCreateInfos.data();
	ci.enabledLayerCount = 0;
	ci.enabledExtensionCount = vkDeviceExtensions.size();
	ci.ppEnabledExtensionNames = vkDeviceExtensions.data();
	ci.pEnabledFeatures = &feats10;

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

EngineResult
VulkanEngine::create_surface() {
	if (SDL_Vulkan_CreateSurface(window, instance, NULL, &surface) != SDL_TRUE) {
		ENGINE_ERROR("Unable to create Vulkan surface from SDL window.");
		return ENGINE_FAILURE;
	}
	return ENGINE_SUCCESS;
}

EngineResult VulkanEngine::create_swapchain(uint32_t width, uint32_t height) {
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
	swapchainExtent = VkExtent2D{ width, height };
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

	return ENGINE_SUCCESS;
}

EngineResult
VulkanEngine::init_swapchain() {
	ENGINE_RUN_FN(create_swapchain(windowExtent.width, windowExtent.height));

	VkExtent3D drawImageExtent = {
		windowExtent.width,
		windowExtent.height,
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

	// depth image
	depthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
	depthImage.imageExtent = drawImageExtent;
	VkImageUsageFlags depthImageUsages{};
	depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	VkImageCreateInfo dimgInfo = { .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	dimgInfo.pNext = NULL;
	dimgInfo.imageType = VK_IMAGE_TYPE_2D;
	dimgInfo.format = depthImage.imageFormat;
	dimgInfo.extent = depthImage.imageExtent;
	dimgInfo.mipLevels = 1;
	dimgInfo.arrayLayers = 1;
	dimgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	dimgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	dimgInfo.usage = depthImageUsages;

	vmaCreateImage(allocator, &dimgInfo, &rImgAllocInfo, &depthImage.image, &depthImage.allocation, NULL);

	VkImageViewCreateInfo dviewInfo = { .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	dviewInfo.pNext = NULL;
	dviewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	dviewInfo.image = depthImage.image;
	dviewInfo.format = depthImage.imageFormat;
	dviewInfo.subresourceRange.baseMipLevel = 0;
	dviewInfo.subresourceRange.levelCount = 1;
	dviewInfo.subresourceRange.baseArrayLayer = 0;
	dviewInfo.subresourceRange.layerCount = 1;
	dviewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

	VK_RUN_FN(deviceDispatch.vkCreateImageView(device, &dviewInfo, nullptr, &depthImage.imageView),
		"Failed to create depth image view");

	mainDeletionQueue.push_function("vkDestroyImageView & vmaDestroyImage for draw and depth images", [=]() {
		deviceDispatch.vkDestroyImageView(device, drawImage.imageView, nullptr);
		vmaDestroyImage(allocator, drawImage.image, drawImage.allocation);

		deviceDispatch.vkDestroyImageView(device, depthImage.imageView, nullptr);
		vmaDestroyImage(allocator, depthImage.image, depthImage.allocation);
		});

	return ENGINE_SUCCESS;
}

EngineResult
VulkanEngine::destroy_swapchain() {
	deviceDispatch.vkDestroySwapchainKHR(device, swapchain, NULL);

	for (size_t i = 0; i < swapchainImgViews.size(); i++) {
		deviceDispatch.vkDestroyImageView(device, swapchainImgViews[i], NULL);
	}
	return ENGINE_SUCCESS;
}

EngineResult
VulkanEngine::resize_swapchain() {
	deviceDispatch.vkDeviceWaitIdle(device);

	if (destroy_swapchain() != ENGINE_SUCCESS) {
		return ENGINE_FAILURE;
	}

	int w, h;
	SDL_GetWindowSize(window, &w, &h);
	windowExtent.width = w;
	windowExtent.height = h;

	ENGINE_RUN_FN(create_swapchain(windowExtent.width, windowExtent.height));

	resizeRequested = false;

	return ENGINE_SUCCESS;
}

EngineResult
VulkanEngine::init_commands() {
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
	mainDeletionQueue.push_function("vkDestroyCommandPool", [=]() {
		deviceDispatch.vkDestroyCommandPool(device, immCmdPool, NULL);
	});

	return ENGINE_SUCCESS;
}

EngineResult
VulkanEngine::init_sync() {
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
	mainDeletionQueue.push_function("vkDestroyFence", [=]() {
		deviceDispatch.vkDestroyFence(device, immFence, NULL);
	});

	return ENGINE_SUCCESS;
}

EngineResult
VulkanEngine::init_descriptors() {
	std::vector<DescriptorAllocator::PoolSizeRatio> sizes =
	{
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }
	};

	descriptorAllocator.init(device, 10, sizes, &deviceDispatch);

	{
		DescriptorLayoutBuilder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, 0);
		drawImageDescriptorLayout = builder.build(device, VK_SHADER_STAGE_COMPUTE_BIT,
			&deviceDispatch);
	}

	// Allocate descriptor set for draw image
	drawImageDescriptors = descriptorAllocator.alloc(device, drawImageDescriptorLayout,
		&deviceDispatch);

	// Write and update the descriptor set
	DescriptorWriter writer;
	writer.write_image(0, drawImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL,
		VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.update_set(device, drawImageDescriptors, &deviceDispatch);

	mainDeletionQueue.push_function("vkDestroyDescriptorSetLayout & destroy_pool", [&] {
		deviceDispatch.vkDestroyDescriptorSetLayout(device, drawImageDescriptorLayout, NULL);
		descriptorAllocator.destroy_pool(device, &deviceDispatch);
		});

	// Set up bindless descriptor sets
	// Bindless layout:
	//		0: COMBINED IMAGE SAMPLERS
	{
		DescriptorLayoutBuilder b;
		b.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000,
			VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
			VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT);
		bindlessDescriptorLayout = b.build(device, VK_SHADER_STAGE_ALL,
			&deviceDispatch);
	}

	std::vector<DescriptorAllocator::PoolSizeRatio> descriptorSizes = {
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
	};
	bindlessDescriptorAllocator.init(device, 1000, descriptorSizes, &deviceDispatch);
	bindlessDescriptorSet = bindlessDescriptorAllocator.alloc(device, 
		bindlessDescriptorLayout, &deviceDispatch);

	mainDeletionQueue.push_function("vkDestroyDescriptorSetLayout & destroy_pool", [&] {
		deviceDispatch.vkDestroyDescriptorSetLayout(device, bindlessDescriptorLayout, NULL);
		bindlessDescriptorAllocator.destroy_pool(device, &deviceDispatch);
	});

	return ENGINE_SUCCESS;
}

EngineResult
VulkanEngine::init_pipelines() {
	if (init_mesh_pipelines() != ENGINE_SUCCESS) {
		return ENGINE_FAILURE;
	}
	if (init_line_pipeline() != ENGINE_SUCCESS) {
		return ENGINE_FAILURE;
	}
	if (init_triangle_pipeline() != ENGINE_SUCCESS) {
		return ENGINE_FAILURE;
	}
	if (init_skybox_pipeline() != ENGINE_SUCCESS) {
		return ENGINE_FAILURE;
	}
	if (init_text_pipeline() != ENGINE_SUCCESS) {
		return ENGINE_FAILURE;
	}
	if (init_wireframe_pipeline() != ENGINE_SUCCESS) {
		return ENGINE_FAILURE;
	}
	return ENGINE_SUCCESS;
}

static PFN_vkVoidFunction
imgui_function_loader(const char* function_name, void* user_data) {
	ImguiLoaderData* loaderData = (ImguiLoaderData*)user_data;
	VkInstance instance = (VkInstance)loaderData->instance;
	InstanceDispatch* instanceDispatch = (InstanceDispatch*)loaderData->instanceDispatch;

	return instanceDispatch->vkGetInstanceProcAddr(instance, function_name);
}

EngineResult
VulkanEngine::init_imgui() {
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
	ImGui_ImplSDL3_InitForVulkan(window);

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

	mainDeletionQueue.push_function("ImGui shutdown", [=]() {
		ImGui_ImplVulkan_Shutdown();
		deviceDispatch.vkDestroyDescriptorPool(device, imguiPool, NULL);
	});

	return ENGINE_SUCCESS;
}

/*  ============================================================================
 *  INIT RESOUCE FUNCTIONS
 *  ============================================================================
 *
 *  These are all functions that are involved in or related
 *  to initializing resources for use by the VulkanEngine.
 */

void
VulkanEngine::shader_monitor_thread() {
	while (!shutdown) {
		shaderMutex.lock();
		for (size_t i = 0; i < shaderCount; i++) {
			if (std::filesystem::exists(shaders[i].path)) {
				if (std::filesystem::last_write_time(shaders[i].path)
					!= shaders[i].lastWrite) {
					printf("Recompile needed\n");
					shaders[i].recompile.store(1);
				}
			}
		}
		shaderMutex.unlock();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}

void
VulkanEngine::recompile_shader(uint32_t idx) {
	shaderMutex.lock();
	if (idx > shaderCount) {
		ENGINE_ERROR("Shader index out of range.");
		shaders[idx].lastWrite = std::filesystem::last_write_time(shaders[idx].path);
		shaders[idx].recompile.store(0);
		shaderMutex.unlock();
		return;
	}

	deviceDispatch.vkDeviceWaitIdle(device);

	// Recompile and recreate shader module
	VkShaderModule oldModule = shaders[idx].shader;

	if (load_shader_module(shaders[idx].path, device,
		&shaders[idx].shader, shaders[idx].stage, &deviceDispatch)
		> 0) {
		ENGINE_WARNING("Could not recompile shader.");
		shaders[idx].lastWrite = std::filesystem::last_write_time(shaders[idx].path);
		shaders[idx].recompile.store(0);
		shaderMutex.unlock();
		return;
	}

	deviceDispatch.vkDestroyShaderModule(device, oldModule, nullptr);

	// Recreate all pipelines that used this shader
	// There is a memory bug here as the pipeline builder's color attachment
	// and depth format are stack allocated pointers in the scope of the builder
	// meaning after the build_pipeline() function is called, these values
	// are not stored in the builder -> builder probably needs some refactoring
	// @TODO ->	REFACTOR BUILDER PLEASE
	for (size_t i = 0; i < pipelineCount; i++) {
		if (pipelines[i].vtxShaderIdx == idx) {
			VkPipeline oldPipeline = pipelines[i].pipeline;
			pipelines[i].builder.set_vtx_shader(shaders[idx].shader);
			pipelines[i].builder.set_color_attachment_format(drawImage.imageFormat);
			pipelines[i].builder.set_depth_format(depthImage.imageFormat);
			pipelines[i].pipeline = pipelines[i].builder.build_pipeline(
				device, &deviceDispatch);
			deviceDispatch.vkDestroyPipeline(device, oldPipeline, nullptr);
		}
		else if (pipelines[i].fragShaderIdx == idx) {
			VkPipeline oldPipeline = pipelines[i].pipeline;
			pipelines[i].builder.set_frag_shader(shaders[idx].shader);
			pipelines[i].builder.set_color_attachment_format(drawImage.imageFormat);
			pipelines[i].builder.set_depth_format(depthImage.imageFormat);
			pipelines[i].pipeline = pipelines[i].builder.build_pipeline(
				device, &deviceDispatch);
			deviceDispatch.vkDestroyPipeline(device, oldPipeline, nullptr);
		}
	}

	shaders[idx].lastWrite = std::filesystem::last_write_time(shaders[idx].path);
	shaderMutex.unlock();
	shaders[idx].recompile.store(0);
}

void
VulkanEngine::destroy_shader(uint32_t idx) {
	shaderMutex.lock();
	deviceDispatch.vkDestroyShaderModule(device, shaders[idx].shader,
		nullptr);

	for (size_t i = idx; i < shaderCount; i++) {
		shaders[i].path = shaders[i + 1].path;
		shaders[i].lastWrite = shaders[i + 1].lastWrite;
		shaders[i].shader = shaders[i + 1].shader;
		shaders[i].stage = shaders[i + 1].stage;
		shaders[i].recompile.store(shaders[i + 1].recompile.load());
	}

	shaderCount--;
	shaderMutex.unlock();
}

EngineResult
VulkanEngine::create_pipeline(PipelineBuilder* builder,
	uint32_t vtxShaderIdx, uint32_t fragShaderIdx, uint32_t* idx) {

	if (pipelineCount >= MAX_PIPELINES) {
		ENGINE_ERROR("Maximum pipelines reached");
		return ENGINE_FAILURE;
	}

	builder->set_vtx_shader(shaders[vtxShaderIdx].shader);
	builder->set_frag_shader(shaders[fragShaderIdx].shader);

	pipelines[pipelineCount].pipeline =
		builder->build_pipeline(device, &deviceDispatch);

	pipelines[pipelineCount].layout = builder->pipelineLayout;

	pipelines[pipelineCount].vtxShaderIdx = vtxShaderIdx;
	pipelines[pipelineCount].fragShaderIdx = fragShaderIdx;
	memcpy(&pipelines[pipelineCount].builder, builder, sizeof(PipelineBuilder));

	*idx = pipelineCount;

	pipelineCount++;

	return ENGINE_SUCCESS;
}

void
VulkanEngine::destroy_pipeline(uint32_t idx) {
	Pipeline* p = &pipelines[idx];
	// Layouts can be shared so we have to make sure
	// that it is valid, and if it is, invalidate
	// any other pipelines that use this layout.
	if (p->layout != VK_NULL_HANDLE) {
		VkPipelineLayout layout = p->layout;
		deviceDispatch.vkDestroyPipelineLayout(device, p->layout, nullptr);
		for (size_t i = 0; i < pipelineCount; i++) {
			if (pipelines[i].layout == layout) {
				pipelines[i].layout = VK_NULL_HANDLE;
			}
		}
	}
	deviceDispatch.vkDestroyPipeline(device, p->pipeline, nullptr);

	for (size_t i = 0; i < pipelineCount; i++) {
		pipelines[i].builder = pipelines[i + 1].builder;
		pipelines[i] = pipelines[i + 1];
	}

	pipelineCount--;
}

EngineResult
VulkanEngine::init_buffers() {
	uint32_t res;
	res = gBuf.create_buffer(device, allocator, GLOBAL_BUFFER_SIZE,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT |
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		0,
		&deviceDispatch);
	if (res > 0) {
		return ENGINE_FAILURE;
	}

	BufferCreateInfo bufferInfo;
	VkBufferDeviceAddressInfo addrInfo;
	addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	addrInfo.pNext = nullptr;

	/*---------------------------
	 |  VERTEX BUFFERS
	 ---------------------------*/
	bufferInfo.allocator = allocator;
	bufferInfo.allocSize = 128 * 1024;
	bufferInfo.flags = 0;
	bufferInfo.pBuffer = &textVertexBuffer;
	bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	create_buffer(&bufferInfo);
	addrInfo.buffer = textVertexBuffer.buffer;
	textVertexBufferAddr =
		deviceDispatch.vkGetBufferDeviceAddress(device, &addrInfo);

	// Create wireframe vertex buffer
	bufferInfo.pBuffer = &wireframeVertexBuffer;
	create_buffer(&bufferInfo);
	addrInfo.buffer = wireframeVertexBuffer.buffer;
	wireframeVertexBufferAddr =
		deviceDispatch.vkGetBufferDeviceAddress(device, &addrInfo);

	// Create line vertex buffer
	bufferInfo.pBuffer = &lineVertexBuffer;
	create_buffer(&bufferInfo);
	addrInfo.buffer = lineVertexBuffer.buffer;
	lineVertexBufferAddr =
		deviceDispatch.vkGetBufferDeviceAddress(device, &addrInfo);

	// Create triangle vertex buffer
	bufferInfo.pBuffer = &triangleVertexBuffer;
	create_buffer(&bufferInfo);
	addrInfo.buffer = triangleVertexBuffer.buffer;
	triangleVertexBufferAddr =
		deviceDispatch.vkGetBufferDeviceAddress(device, &addrInfo);

	/*---------------------------
	 |  INDEX BUFFERS
	 ---------------------------*/
	// Create text index buffer
	bufferInfo.pBuffer = &textIndexBuffer;
	bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	create_buffer(&bufferInfo);

	// Create wireframe index buffer
	bufferInfo.pBuffer = &wireframeIndexBuffer;
	create_buffer(&bufferInfo);

	/*---------------------------
	 |  UNIFORM BUFFERS
	 ---------------------------*/
	// Create scene data UBO
	bufferInfo.pBuffer = &uSceneData;
	bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	bufferInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
		VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
	bufferInfo.allocSize = sizeof(GPUSceneData);
	create_buffer(&bufferInfo);
	addrInfo.buffer = uSceneData.buffer;
	uSceneDataAddr =
		deviceDispatch.vkGetBufferDeviceAddress(device, &addrInfo);

	// LIGHT BUFFERS

	bufferInfo.pBuffer = &uLightData;
	bufferInfo.allocSize = sizeof(GPULightData);
	create_buffer(&bufferInfo);
	addrInfo.buffer = uLightData.buffer;
	uLightDataAddr =
		deviceDispatch.vkGetBufferDeviceAddress(device, &addrInfo);

	bufferInfo.pBuffer = &uDirectionalLights;
	bufferInfo.allocSize = MAX_DIR_LIGHTS * sizeof(DirectionalLight);
	create_buffer(&bufferInfo);
	addrInfo.buffer = uDirectionalLights.buffer;
	uDirectionalLightsAddr =
		deviceDispatch.vkGetBufferDeviceAddress(device, &addrInfo);

	bufferInfo.pBuffer = &uPointLights;
	bufferInfo.allocSize = MAX_POINT_LIGHTS * sizeof(PointLight);
	create_buffer(&bufferInfo);
	addrInfo.buffer = uPointLights.buffer;
	uPointLightsAddr =
		deviceDispatch.vkGetBufferDeviceAddress(device, &addrInfo);

	bufferInfo.pBuffer = &uSpotLights;
	bufferInfo.allocSize = MAX_SPOT_LIGHTS * sizeof(SpotLight);
	create_buffer(&bufferInfo);
	addrInfo.buffer = uSpotLights.buffer;
	uSpotLightsAddr =
		deviceDispatch.vkGetBufferDeviceAddress(device, &addrInfo);

	GPULightData* lightData = (GPULightData*)uLightData.info.pMappedData;
	lightData->directionalLights = uDirectionalLightsAddr;
	lightData->pointLights = uPointLightsAddr;
	lightData->spotLights = uSpotLightsAddr;

	bufferInfo.pBuffer = &uMaterialBuffer;
	bufferInfo.allocSize = MAX_MATERIALS * sizeof(Material);
	create_buffer(&bufferInfo);
	addrInfo.buffer = uMaterialBuffer.buffer;
	uMaterialBufferAddr =
		deviceDispatch.vkGetBufferDeviceAddress(device, &addrInfo);

	mainDeletionQueue.push_function("destroying base buffers",
		[&]() {
			gBuf.destroy_buffer(allocator);
			destroy_buffer(&triangleVertexBuffer);
			destroy_buffer(&lineVertexBuffer);
			destroy_buffer(&uLightData);
			destroy_buffer(&uPointLights);
			destroy_buffer(&uDirectionalLights);
			destroy_buffer(&uSpotLights);
			destroy_buffer(&textVertexBuffer);
			destroy_buffer(&textIndexBuffer);
			destroy_buffer(&wireframeVertexBuffer);
			destroy_buffer(&wireframeIndexBuffer);
			destroy_buffer(&uSceneData);
			destroy_buffer(&uMaterialBuffer);
		});

	return ENGINE_SUCCESS;
}

EngineResult
VulkanEngine::init_mesh_pipelines() {
	// Declare push constant buffer range
	VkPushConstantRange bufferRange{};
	bufferRange.offset = 0;
	bufferRange.size = sizeof(GPUDrawPushConstants);
	bufferRange.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;

	// Create pipeline layout
	VkPipelineLayoutCreateInfo layoutCi = { .sType =
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	layoutCi.pNext = nullptr;
	layoutCi.pPushConstantRanges = &bufferRange;
	layoutCi.pushConstantRangeCount = 1;
	// Use bindless layout
	layoutCi.pSetLayouts = &bindlessDescriptorLayout;
	layoutCi.setLayoutCount = 1;

	VkPipelineLayout layout;

	VK_RUN_FN(deviceDispatch.vkCreatePipelineLayout(device, &layoutCi, nullptr,
		&layout));

	uint32_t vtxShader, fragShader;
	if (create_shader("../../shaders/mesh.vert",
		EShLangVertex, &vtxShader) != ENGINE_SUCCESS) {
		return ENGINE_FAILURE;
	}
	if (create_shader("../../shaders/mesh.frag",
		EShLangFragment, &fragShader) != ENGINE_SUCCESS) {
		return ENGINE_FAILURE;
	}

	PipelineBuilder builder;

	builder.set_layout(layout);
	builder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	builder.set_polygon_mode(VK_POLYGON_MODE_FILL);
	builder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	builder.set_multisampling_none();
	builder.disable_blending();
	builder.enable_depthtest(true, VK_COMPARE_OP_LESS);

	builder.set_color_attachment_format(drawImage.imageFormat);
	builder.set_depth_format(depthImage.imageFormat);

	create_pipeline(&builder, vtxShader, fragShader, &opaquePipeline);

	builder.enable_depthtest(false, VK_COMPARE_OP_GREATER_OR_EQUAL);
	builder.enable_blending_additive();

	create_pipeline(&builder, vtxShader, fragShader, &transparentPipeline);

	return ENGINE_SUCCESS;
}

EngineResult
VulkanEngine::init_line_pipeline() {
	// For lines, we are only interested in sending the scene data
	// and a vertex buffer (list of lines)
	VkPushConstantRange bufferRange{};
	bufferRange.offset = 0;
	bufferRange.size = 2 * sizeof(VkDeviceAddress);
	bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkPipelineLayoutCreateInfo layoutCi = { .sType =
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	layoutCi.pNext = nullptr;
	layoutCi.pPushConstantRanges = &bufferRange;
	layoutCi.pushConstantRangeCount = 1;
	layoutCi.pSetLayouts = nullptr;
	layoutCi.setLayoutCount = 0;

	VkPipelineLayout layout;
	VK_RUN_FN(deviceDispatch.vkCreatePipelineLayout(device, &layoutCi, nullptr, &layout));

	uint32_t vtxShader, fragShader;
	if (create_shader("../../shaders/line.vert",
		EShLangVertex, &vtxShader) != ENGINE_SUCCESS) {
		return ENGINE_FAILURE;
	}
	if (create_shader("../../shaders/line.frag",
		EShLangFragment, &fragShader) != ENGINE_SUCCESS) {
		return ENGINE_FAILURE;
	}

	PipelineBuilder builder;

	builder.set_layout(layout);
	builder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
	builder.set_polygon_mode(VK_POLYGON_MODE_LINE);
	builder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	builder.set_multisampling_none();
	builder.disable_blending();
	builder.enable_depthtest(true, VK_COMPARE_OP_LESS);

	builder.set_color_attachment_format(drawImage.imageFormat);
	builder.set_depth_format(depthImage.imageFormat);

	builder.add_dynamic_state(VK_DYNAMIC_STATE_LINE_WIDTH);

	create_pipeline(&builder, vtxShader, fragShader, &linePipeline);

	return ENGINE_SUCCESS;
}

EngineResult
VulkanEngine::init_triangle_pipeline() {
	// For triangles, we are only interested in sending the scene data
	// and a vertex buffer (list of lines)
	VkPushConstantRange bufferRange{};
	bufferRange.offset = 0;
	bufferRange.size = 2 * sizeof(VkDeviceAddress);
	bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkPipelineLayoutCreateInfo layoutCi = { .sType =
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	layoutCi.pNext = nullptr;
	layoutCi.pPushConstantRanges = &bufferRange;
	layoutCi.pushConstantRangeCount = 1;
	layoutCi.pSetLayouts = nullptr;
	layoutCi.setLayoutCount = 0;

	VkPipelineLayout layout;
	VK_RUN_FN(deviceDispatch.vkCreatePipelineLayout(device, &layoutCi, nullptr, &layout));

	uint32_t vtxShader, fragShader;
	if (create_shader("../../shaders/triangle.vert",
		EShLangVertex, &vtxShader) != ENGINE_SUCCESS) {
		return ENGINE_FAILURE;
	}
	if (create_shader("../../shaders/triangle.frag",
		EShLangFragment, &fragShader) != ENGINE_SUCCESS) {
		return ENGINE_FAILURE;
	}

	PipelineBuilder builder;

	builder.set_layout(layout);
	builder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	builder.set_polygon_mode(VK_POLYGON_MODE_FILL);
	builder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	builder.set_multisampling_none();
	builder.disable_blending();
	builder.enable_depthtest(true, VK_COMPARE_OP_LESS);

	builder.set_color_attachment_format(drawImage.imageFormat);
	builder.set_depth_format(depthImage.imageFormat);

	create_pipeline(&builder, vtxShader, fragShader, &trianglePipeline);

	return ENGINE_SUCCESS;
}

EngineResult
VulkanEngine::init_wireframe_pipeline() {
	VkPushConstantRange pcRange = {};
	pcRange.offset = 0;
	pcRange.size = sizeof(GPUDrawPushConstants);
	pcRange.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;

	VkPipelineLayoutCreateInfo layoutCi = {};
	layoutCi.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutCi.pNext = nullptr;

	layoutCi.pPushConstantRanges = &pcRange;
	layoutCi.pushConstantRangeCount = 1;

	layoutCi.pSetLayouts = &bindlessDescriptorLayout;
	layoutCi.setLayoutCount = 1;

	VkPipelineLayout layout;
	if (deviceDispatch.vkCreatePipelineLayout(device, &layoutCi, nullptr,
		&layout) != VK_SUCCESS) {
		ENGINE_ERROR("Failed to create wireframe pipeline layout.");
		return ENGINE_FAILURE;
	}

	uint32_t vtxShader, fragShader;
	if (create_shader("../../shaders/wireframe.vert", EShLangVertex,
		&vtxShader) != ENGINE_SUCCESS) {
		return ENGINE_FAILURE;
	}
	if (create_shader("../../shaders/wireframe.frag", EShLangFragment,
		&fragShader) != ENGINE_SUCCESS) {
		return ENGINE_FAILURE;
	}

	PipelineBuilder builder;
	builder.set_layout(layout);
	builder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	builder.set_polygon_mode(VK_POLYGON_MODE_LINE);
	builder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	builder.set_multisampling_none();
	builder.disable_blending();
	
	builder.set_depth_format(depthImage.imageFormat);
	builder.enable_depthtest(true, VK_COMPARE_OP_LESS_OR_EQUAL);

	builder.set_color_attachment_format(drawImage.imageFormat);

	builder.add_dynamic_state(VK_DYNAMIC_STATE_LINE_WIDTH);
	builder.add_dynamic_state(VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE);

	create_pipeline(&builder, vtxShader, fragShader, &wireframePipeline);

	return ENGINE_SUCCESS;
}

EngineResult
VulkanEngine::init_skybox_pipeline() {
	// Declare push constant buffer range
	VkPushConstantRange bufferRange{};
	bufferRange.offset = 0;
	bufferRange.size = sizeof(GPUDrawPushConstants);
	bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	// Create pipeline layout
	VkPipelineLayoutCreateInfo layoutCi = { .sType =
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	layoutCi.pNext = nullptr;
	layoutCi.pPushConstantRanges = &bufferRange;
	layoutCi.pushConstantRangeCount = 1;
	// Use bindless layout
	layoutCi.pSetLayouts = &bindlessDescriptorLayout;
	layoutCi.setLayoutCount = 1;

	VkPipelineLayout layout;
	VK_RUN_FN(deviceDispatch.vkCreatePipelineLayout(device, &layoutCi, nullptr,
		&layout));

	uint32_t vtxShader, fragShader;
	if (create_shader("../../shaders/skybox.vert",
		EShLangVertex, &vtxShader) != ENGINE_SUCCESS) {
		return ENGINE_FAILURE;
	}
	if (create_shader("../../shaders/skybox.frag",
		EShLangFragment, &fragShader) != ENGINE_SUCCESS) {
		return ENGINE_FAILURE;
	}

	PipelineBuilder builder;

	builder.set_layout(layout);
	builder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	builder.set_polygon_mode(VK_POLYGON_MODE_FILL);
	builder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	builder.set_multisampling_none();
	builder.disable_blending();
	builder.enable_depthtest(true, VK_COMPARE_OP_LESS_OR_EQUAL);

	builder.set_color_attachment_format(drawImage.imageFormat);
	builder.set_depth_format(depthImage.imageFormat);

	create_pipeline(&builder, vtxShader, fragShader, &skyboxPipeline);

	return ENGINE_SUCCESS;
}

EngineResult
VulkanEngine::init_text_pipeline() {
	// Declare push constant buffer range
	VkPushConstantRange bufferRange{};
	bufferRange.offset = 0;
	bufferRange.size = sizeof(GPUDrawPushConstants);
	bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	// Create pipeline layout
	VkPipelineLayoutCreateInfo layoutCi = { .sType =
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	layoutCi.pNext = nullptr;
	layoutCi.pPushConstantRanges = &bufferRange;
	layoutCi.pushConstantRangeCount = 1;
	// Use bindless layout
	layoutCi.pSetLayouts = &bindlessDescriptorLayout;
	layoutCi.setLayoutCount = 1;

	VkPipelineLayout layout;
	VK_RUN_FN(deviceDispatch.vkCreatePipelineLayout(device, &layoutCi, nullptr,
		&layout));

	uint32_t vtxShader, fragShader;
	if (create_shader("../../shaders/text.vert",
		EShLangVertex, &vtxShader) != ENGINE_SUCCESS) {
		return ENGINE_FAILURE;
	}
	if (create_shader("../../shaders/text.frag",
		EShLangFragment, &fragShader) != ENGINE_SUCCESS) {
		return ENGINE_FAILURE;
	}

	PipelineBuilder builder;

	builder.set_layout(layout);
	builder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	builder.set_polygon_mode(VK_POLYGON_MODE_FILL);
	builder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	builder.set_multisampling_none();
	builder.enable_blending_alphablend();
	builder.disable_depthtest();

	builder.set_color_attachment_format(drawImage.imageFormat);


	create_pipeline(&builder, vtxShader, fragShader, &textPipeline);

	return ENGINE_SUCCESS;
}

EngineResult
VulkanEngine::init_default_data() {
	// load example gltf meshes
	loadGltfMeshes(this, "../../assets/basicmesh.glb");

	uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
	// checkerboard image
	uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
	uint32_t pixels[16 * 16] = { 0 }; // for 16x16 checkerboard pattern
	for (size_t x = 0; x < 16; x++) {
		for (size_t y = 0; y < 16; y++) {
			pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
		}
	}

	ImageCreateInfo imgInfo = {};
	imgInfo.allocator = allocator;
	imgInfo.device = device;
	imgInfo.pDeviceDispatch = &deviceDispatch;
	imgInfo.pImg = &errorCheckerboardImage;
	imgInfo.size = VkExtent3D{ 16, 16, 1 };
	imgInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	imgInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;

	VkCommandBuffer cmd = {};

	create_image_with_data(&imgInfo, pixels, immCmdBuf, immFence, graphicsQueue);

	VkSamplerCreateInfo sampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	sampl.magFilter = VK_FILTER_NEAREST;
	sampl.minFilter = VK_FILTER_NEAREST;

	deviceDispatch.vkCreateSampler(device, &sampl, nullptr, &defaultSamplerNearest);

	sampl.magFilter = VK_FILTER_LINEAR;
	sampl.minFilter = VK_FILTER_LINEAR;
	deviceDispatch.vkCreateSampler(device, &sampl, nullptr, &defaultSamplerLinear);

	errorHandle = bindlessDescriptorWriter.write_image(
		0,
		errorCheckerboardImage.imageView,
		defaultSamplerNearest,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
	);

	// load space cube map texture

	imgInfo.pImg = &spaceCubeMap;
	create_image_cube_map(&imgInfo, "../../assets/space_cube/", immCmdBuf,
		immFence, graphicsQueue);

	errorHandle = bindlessDescriptorWriter.write_image(
		0,
		spaceCubeMap.imageView,
		defaultSamplerNearest,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
	);

	int width, height, channels;
	stbi_uc* data = stbi_load("../../assets/cubemap.png", &width, &height,
		&channels, STBI_rgb_alpha);

	imgInfo.pImg = &containerTexture;
	imgInfo.size = { 500, 500, 1 };
	create_image_with_data(&imgInfo, data, immCmdBuf, immFence, graphicsQueue);
	bindlessDescriptorWriter.write_image(
		0,
		containerTexture.imageView,
		defaultSamplerNearest,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
	);

	FontCreateInfo fontInfo;
	fontInfo.allocator = allocator;
	fontInfo.device = device;
	fontInfo.cmd = immCmdBuf;
	fontInfo.fence = immFence;
	fontInfo.queue = graphicsQueue;
	fontInfo.pDeviceDispatch = &deviceDispatch;
	fontInfo.ttfPath = "../../assets/fonts/Roboto-Regular.ttf";
	fontInfo.size = 32;
	defaultFont.create(&fontInfo);

	uint32_t idx = 0;
	idx = bindlessDescriptorWriter.write_image(0, defaultFont.texture.imageView,
		defaultSamplerLinear, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

	defaultFont.descriptorIndex = idx;


	bindlessDescriptorWriter.update_set(device, bindlessDescriptorSet,
		&deviceDispatch);

	Material copper{
		.ambient = glm::vec3{ 0.19125, 0.0735, 0.0225 },
		.diffuse = glm::vec3{ 0.7038, 0.27048, 0.0828 },
		.specular = glm::vec3{ 	0.256777, 0.137622, 0.086014 },
		.shininess = 8
	};
	uint32_t copperIdx;
	create_material(&copper, &copperIdx);

	directionalLights.push_back(DirectionalLight{
		.direction = glm::vec3(0.5f, -1.f, 0.5f),
		.ambient = glm::vec3(0.1f, 0.1f, 0.1f),
		.diffuse = glm::vec3(0.3f, 0.1f, 0.1f),
		.specular = glm::vec3(1.f, 1.f, 1.f)
		});
	pointLights.push_back(PointLight{
		.position = glm::vec3(6.f, 3.f, 2.f),
		.ambient = glm::vec3(0.1f, 0.1f, 0.1f),
		.diffuse = glm::vec3(1.f, 0.f, 1.f),
		.specular = glm::vec3(0.5f, 0.5f, 0.5f),
		.constant = 1.0f,
		.linear = 0.09f,
		.quadratic = 0.032f
		});

	memcpy(uPointLights.info.pMappedData, &pointLights[0],
		sizeof(PointLight));
	memcpy(uDirectionalLights.info.pMappedData, &directionalLights[0],
		sizeof(DirectionalLight));
	GPULightData* lightData = (GPULightData*)uLightData.info.pMappedData;
	lightData->numDirectionalLights = 1;
	lightData->numPointLights = 1;

	mainDeletionQueue.push_function("Default textures and samplers deletion", [&]() {
		deviceDispatch.vkDestroySampler(device, defaultSamplerLinear, nullptr);
		deviceDispatch.vkDestroySampler(device, defaultSamplerNearest, nullptr);

		destroy_image(device, &deviceDispatch, allocator, &errorCheckerboardImage);
		destroy_image(device, &deviceDispatch, allocator, &spaceCubeMap);
		destroy_image(device, &deviceDispatch, allocator, &containerTexture);
		defaultFont.destroy(device, &deviceDispatch, allocator);
		});

	return ENGINE_SUCCESS;
}

/*  ============================================================================
 *  DRAW FUNCTIONS
 *  ============================================================================
 *
 *  These are functions that are related to drawing and/or
 *  are called frequently (per frame or otherwise).
 */

void
VulkanEngine::begin() {
	// ImGui
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();
	_mainDrawContext.clear();
}

EngineResult
VulkanEngine::draw() {
	if (deviceDispatch.vkWaitForFences(device, 1, &get_current_frame().renderFence, true, TIMEOUT_N) != VK_SUCCESS) {
		ENGINE_ERROR("Failed to wait for fences (draw).");
		return ENGINE_FAILURE;
	}
	get_current_frame().deletionQueue.flush();

	for (size_t i = 0; i < shaderCount; i++) {
		if (shaders[i].recompile.load()) {
			recompile_shader(i);
		}
	}

	drawExtent.width = std::min(drawImage.imageExtent.width, swapchainExtent.width) * renderScale;
	drawExtent.height = std::min(drawImage.imageExtent.height, swapchainExtent.height) * renderScale;

	uint32_t swapchainImgIndex;
	VkResult e = deviceDispatch.vkAcquireNextImageKHR(device, swapchain, TIMEOUT_N, get_current_frame().swapchainSemaphore,
		NULL, &swapchainImgIndex);
	if (e == VK_ERROR_OUT_OF_DATE_KHR) {
		resizeRequested = true;
		return ENGINE_SUCCESS;
	}

	if (deviceDispatch.vkResetFences(device, 1, &get_current_frame().renderFence) != VK_SUCCESS) {
		ENGINE_ERROR("Failed to reset fences.");
		return ENGINE_FAILURE;
	}

	VkCommandBuffer cmd = get_current_frame().cmdBuf;
	if (deviceDispatch.vkResetCommandBuffer(cmd, 0) != VK_SUCCESS) {
		ENGINE_ERROR("Failed to reset command buffer.");
		return ENGINE_FAILURE;
	}

	//>> Draw first

	VkCommandBufferBeginInfo cmdBi = {};
	cmdBi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBi.pNext = NULL;
	cmdBi.pInheritanceInfo = NULL;
	cmdBi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	if (deviceDispatch.vkBeginCommandBuffer(cmd, &cmdBi) != VK_SUCCESS) {
		ENGINE_ERROR("Failed to begin command buffer.");
		return ENGINE_FAILURE;
	}

	transition_image(cmd, drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, &deviceDispatch);

	transition_image(cmd, depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, &deviceDispatch);

	// Write to scene data
	GPUSceneData* data = (GPUSceneData*)uSceneData.info.pMappedData;
	sceneData.view = _activeCamera.calcViewMat();
	sceneData.proj = glm::perspective(
		glm::radians(45.f),
		(float)drawExtent.width / (float)drawExtent.height,
		0.1f, 1000.f
	);
	sceneData.proj[1][1] *= -1;
	sceneData.orthoProj = glm::ortho(
		0.0f, (float)drawExtent.width / renderScale, 0.0f,
		(float)drawExtent.height / renderScale, -1.0f, 1.0f
	);
	*data = sceneData;

	render_main_pass(cmd);
	if (_debugFlags & DEBUG_FLAGS_ENABLE) {
		render_debug_pass(cmd);
	}

	// Transition the draw image and the swapchain image into their correct trnasfer layouts
	transition_image(cmd, drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		&deviceDispatch);
	transition_image(cmd, swapchainImgs[swapchainImgIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		&deviceDispatch);
	//
	//>> Draw imgui
	copy_image_to_image(cmd, drawImage.image, swapchainImgs[swapchainImgIndex], drawExtent, swapchainExtent,
		&deviceDispatch);

	transition_image(cmd, swapchainImgs[swapchainImgIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, &deviceDispatch);

	render_imgui(cmd, swapchainImgViews[swapchainImgIndex]);

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

	VkResult res = deviceDispatch.vkQueuePresentKHR(graphicsQueue, &presentInfo);
	if (res == VK_ERROR_OUT_OF_DATE_KHR) {
		resizeRequested = true;
	}

	frameNumber++;
	return ENGINE_SUCCESS;
}


EngineResult
VulkanEngine::immediate_submit(std::function<void(VkCommandBuffer cmd)>&& fn) {
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
		"Failed to wait for fences (imm)");

	return ENGINE_SUCCESS;
}

EngineResult
VulkanEngine::render_imgui(VkCommandBuffer cmd, VkImageView targetImgView) {
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

	ImGui::Render();

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

	deviceDispatch.vkCmdEndRendering(cmd);

	return ENGINE_SUCCESS;
}

EngineResult
VulkanEngine::render_main_pass(VkCommandBuffer cmd) {
	VkRenderingAttachmentInfo colorAttachment = {};
	colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	colorAttachment.pNext = NULL;

	colorAttachment.imageView = drawImage.imageView;
	colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	VkRenderingAttachmentInfo depthAttachment = {};
	depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	depthAttachment.pNext = NULL;

	depthAttachment.imageView = depthImage.imageView;
	depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depthAttachment.clearValue.depthStencil.depth = 1.f;


	VkRenderingInfo renderInfo = {};
	renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderInfo.pNext = NULL;

	renderInfo.renderArea = VkRect2D{ VkOffset2D { 0, 0 }, drawExtent };
	renderInfo.layerCount = 1;
	renderInfo.colorAttachmentCount = 1;
	renderInfo.pColorAttachments = &colorAttachment;
	renderInfo.pDepthAttachment = &depthAttachment;
	renderInfo.pStencilAttachment = NULL;

	deviceDispatch.vkCmdBeginRendering(cmd, &renderInfo);

	VkViewport viewport = {};
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = drawExtent.width;
	viewport.height = drawExtent.height;
	viewport.minDepth = 0.f;
	viewport.maxDepth = 1.f;

	deviceDispatch.vkCmdSetViewport(cmd, 0, 1, &viewport);

	VkRect2D scissor = {};
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = drawExtent.width;
	scissor.extent.height = drawExtent.height;

	deviceDispatch.vkCmdSetScissor(cmd, 0, 1, &scissor);

	//render_geometry(cmd);
	render_skybox(cmd);
	//render_wireframes(cmd);
	//render_lines(cmd);
	//render_triangles(cmd);

	deviceDispatch.vkCmdEndRendering(cmd);

	return ENGINE_SUCCESS;
}

EngineResult
VulkanEngine::render_geometry(VkCommandBuffer cmd) {
	Pipeline p = pipelines[opaquePipeline];

	deviceDispatch.vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 
		p.pipeline);

	deviceDispatch.vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
		p.layout, 0, 1, &bindlessDescriptorSet, 0, nullptr);
	

	for (size_t i = 0; i < _mainDrawContext._surfaceData.size(); i++) {
		const SurfaceDrawData* surface = &_mainDrawContext._surfaceData[i];
		deviceDispatch.vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipelines[opaquePipeline].pipeline);
		deviceDispatch.vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipelines[opaquePipeline].layout, 0, 1, &bindlessDescriptorSet,
			0, 0);

		deviceDispatch.vkCmdBindIndexBuffer(cmd, gBuf.buffer, surface->indexBufferAddr, 
			VK_INDEX_TYPE_UINT32);

		GPUDrawPushConstants pc;
		pc.vertexBuffer = gBuf.addr + surface->vertexBufferAddr;
		pc.sceneBuffer = uSceneDataAddr;
		pc.materialBuffer = uMaterialBufferAddr;
		pc.lightBuffer = uLightDataAddr;
		pc.materialID = surface->materialID;
		pc.model = surface->transform;
		pc.viewPos = _activeCamera.pos;
		deviceDispatch.vkCmdPushConstants(cmd, pipelines[opaquePipeline].layout,
			VK_SHADER_STAGE_ALL_GRAPHICS, 0, sizeof(GPUDrawPushConstants), &p);

		deviceDispatch.vkCmdDrawIndexed(cmd, surface->indexCount, 1, surface->firstIndex, 0, 0);
	}

	return ENGINE_SUCCESS;
}

EngineResult
VulkanEngine::render_skybox(VkCommandBuffer cmd) {

	deviceDispatch.vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
		pipelines[skyboxPipeline].pipeline);
	deviceDispatch.vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
		pipelines[skyboxPipeline].layout, 0, 1, &bindlessDescriptorSet, 0, nullptr);

	glm::mat4 view = _activeCamera.calcViewMat();
	glm::mat4 proj = glm::perspective(
		glm::radians(45.f),
		(float)drawExtent.width / (float)drawExtent.height,
		0.1f, 1000.f
	);
	proj[1][1] *= -1;

	GPUDrawPushConstants pushConstants;
	pushConstants.vertexBuffer = gBuf.addr + meshes[0].vertexOffset;
	pushConstants.sceneBuffer = uSceneDataAddr;
	pushConstants.model = glm::mat4(0);
	deviceDispatch.vkCmdPushConstants(cmd, pipelines[skyboxPipeline].layout,
		VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &pushConstants);

	deviceDispatch.vkCmdBindIndexBuffer(cmd, gBuf.buffer, meshes[0].indexOffset,
		VK_INDEX_TYPE_UINT32);

	deviceDispatch.vkCmdDrawIndexed(cmd, meshes[0].surfaces[0].count,
		1, meshes[0].surfaces[0].startIndex, 0, 0);

	return ENGINE_SUCCESS;
}

EngineResult
VulkanEngine::render_debug_pass(VkCommandBuffer cmd) {
	VkRenderingAttachmentInfo colorAttachment = {};
	colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	colorAttachment.pNext = nullptr;

	colorAttachment.imageView = drawImage.imageView;
	colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	VkRenderingInfo renderInfo = {};
	renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderInfo.pNext = nullptr;

	renderInfo.renderArea = VkRect2D{ VkOffset2D { 0, 0 }, drawExtent };
	renderInfo.layerCount = 1;
	renderInfo.colorAttachmentCount = 1;
	renderInfo.pColorAttachments = &colorAttachment;

	deviceDispatch.vkCmdBeginRendering(cmd, &renderInfo);

	// Render scene geometry wireframe if enabled
	if (_debugFlags & DEBUG_FLAGS_GEOMETRY_WIREFRAME) {
		Pipeline p = pipelines[wireframePipeline];
		deviceDispatch.vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, p.pipeline);
		deviceDispatch.vkCmdSetDepthTestEnable(cmd, VK_FALSE);
		deviceDispatch.vkCmdSetLineWidth(cmd, 1.f);
		for (size_t i = 0; i < _mainDrawContext._surfaceData.size(); i++) {
			const SurfaceDrawData* surface = &_mainDrawContext._surfaceData[i];
			deviceDispatch.vkCmdBindIndexBuffer(cmd, gBuf.buffer, surface->indexBufferAddr,
				VK_INDEX_TYPE_UINT32);

			GPUDrawPushConstants pc;
			pc.vertexBuffer = gBuf.addr + surface->vertexBufferAddr;
			pc.sceneBuffer = uSceneDataAddr;
			pc.model = surface->transform;
			deviceDispatch.vkCmdPushConstants(cmd, p.layout, VK_SHADER_STAGE_ALL_GRAPHICS,
				0, sizeof(GPUDrawPushConstants), &pc);
			deviceDispatch.vkCmdDrawIndexed(cmd, surface->indexCount, 1, surface->firstIndex, 0, 0);
		}
	}

	deviceDispatch.vkCmdEndRendering(cmd);

	if (ImGui::Begin("Renderer Debugging")) {
		ImGui::SliderFloat("Render Scale", &renderScale, 0.3f, 1.0);
		ImGui::CheckboxFlags("Geometry Wireframe", &_debugFlags, DEBUG_FLAGS_GEOMETRY_WIREFRAME);
	}
	ImGui::End();

	return ENGINE_SUCCESS;
}


EngineResult
VulkanEngine::render_lines(VkCommandBuffer cmd) {
	// Nothing to draw if no vertices recorded in lineCtx
	if (_mainDrawContext._lineData.size() <= 0) {
		return ENGINE_SUCCESS;
	}

	LineVertex* lineData = _mainDrawContext._lineData.data();
	size_t vtxSize = _mainDrawContext._lineData.size() * sizeof(LineVertex);

	void* data = (void*)lineData;

	CopyDataToBufferInfo copyInfo;
	copyInfo.buffer = lineVertexBuffer.buffer;
	copyInfo.allocator = allocator;
	copyInfo.pData = data;
	copyInfo.size = vtxSize;
	copyInfo.device = device;
	copyInfo.pDeviceDispatch = &deviceDispatch;
	copyInfo.cmdBuf = immCmdBuf;
	copyInfo.cmdFence = immFence;
	copyInfo.queue = graphicsQueue;
	copy_data_to_buffer(&copyInfo);

	Pipeline p = pipelines[linePipeline];

	deviceDispatch.vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
		p.pipeline);
	deviceDispatch.vkCmdSetLineWidth(cmd, 1);

	GPUDrawPushConstants pc;
	pc.sceneBuffer = uSceneDataAddr;
	pc.vertexBuffer = lineVertexBufferAddr;

	deviceDispatch.vkCmdPushConstants(cmd, p.layout,
		VK_SHADER_STAGE_VERTEX_BIT, 0, 
		2 * sizeof(VkDeviceAddress), &pc);
	deviceDispatch.vkCmdDraw(cmd, _mainDrawContext._lineData.size(), 1, 0, 0);
}

EngineResult
VulkanEngine::render_triangles(VkCommandBuffer cmd) {
	if (_mainDrawContext._triangleData.size() <= 0) {
		return ENGINE_SUCCESS;
	}

	void* data = (void*)_mainDrawContext._triangleData.data();
	size_t vtxSize = _mainDrawContext._triangleData.size() * sizeof(TriangleVertex);

	CopyDataToBufferInfo copyInfo;
	copyInfo.buffer = triangleVertexBuffer.buffer;
	copyInfo.allocator = allocator;
	copyInfo.pData = data;
	copyInfo.size = vtxSize;
	copyInfo.device = device;
	copyInfo.pDeviceDispatch = &deviceDispatch;
	copyInfo.cmdBuf = immCmdBuf;
	copyInfo.cmdFence = immFence;
	copyInfo.queue = graphicsQueue;
	copy_data_to_buffer(&copyInfo);

	Pipeline p = pipelines[trianglePipeline];

	deviceDispatch.vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
		p.pipeline);

	GPUDrawPushConstants pc;
	pc.sceneBuffer = uSceneDataAddr;
	pc.vertexBuffer = triangleVertexBufferAddr;

	deviceDispatch.vkCmdPushConstants(cmd, p.layout,
		VK_SHADER_STAGE_VERTEX_BIT, 0,
		2 * sizeof(VkDeviceAddress), &pc);
	deviceDispatch.vkCmdDraw(cmd, _mainDrawContext._triangleData.size(), 1, 0, 0);
}

EngineResult
VulkanEngine::render_wireframes(VkCommandBuffer cmd) {
	Pipeline p = pipelines[wireframePipeline];

	deviceDispatch.vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
		p.pipeline);
	deviceDispatch.vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
		p.layout, 0, 1, &bindlessDescriptorSet, 0, nullptr);
	deviceDispatch.vkCmdSetLineWidth(cmd, 2.0);

	if (_mainDrawContext._wireframeData.indices.size() > 0) {
		size_t vtxSize = _mainDrawContext._wireframeData.vertices.size() * sizeof(Vertex);
		size_t idxSize = _mainDrawContext._wireframeData.indices.size() * sizeof(uint32_t);
		AllocatedBuffer uploadBuffer;
		BufferCreateInfo bufferInfo;
		bufferInfo.allocator = allocator;
		bufferInfo.allocSize = vtxSize + idxSize;
		bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		bufferInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
			VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
		bufferInfo.pBuffer = &uploadBuffer;
		create_buffer(&bufferInfo);

		void* data = uploadBuffer.info.pMappedData;
		memcpy(data, _mainDrawContext._wireframeData.vertices.data(), vtxSize);
		memcpy((char*)data + vtxSize, _mainDrawContext._wireframeData.indices.data(), idxSize);


		immediate_submit([&](VkCommandBuffer cmd) {
			VkBufferCopy vertexCopy{ 0 };
			vertexCopy.dstOffset = 0;
			vertexCopy.srcOffset = 0;
			vertexCopy.size = vtxSize;

			deviceDispatch.vkCmdCopyBuffer(cmd, uploadBuffer.buffer,
				wireframeVertexBuffer.buffer, 1, &vertexCopy);

			VkBufferCopy indexCopy{ 0 };
			indexCopy.dstOffset = 0;
			indexCopy.srcOffset = vtxSize;
			indexCopy.size = idxSize;

			deviceDispatch.vkCmdCopyBuffer(cmd, uploadBuffer.buffer,
				wireframeIndexBuffer.buffer, 1, &indexCopy);
			});

		destroy_buffer(&uploadBuffer);

		deviceDispatch.vkCmdBindIndexBuffer(cmd, wireframeIndexBuffer.buffer,
			0, VK_INDEX_TYPE_UINT32);
		GPUDrawPushConstants pc;
		pc.vertexBuffer = wireframeVertexBufferAddr;
		pc.sceneBuffer = uSceneDataAddr;
		pc.model = glm::mat4(1.f);

		deviceDispatch.vkCmdPushConstants(cmd, p.layout,
			VK_SHADER_STAGE_ALL_GRAPHICS, 0, sizeof(GPUDrawPushConstants), &pc);

		deviceDispatch.vkCmdDrawIndexed(cmd, _mainDrawContext._wireframeData.indices.size(),
			1, 0, 0, 0);
	}

	return ENGINE_SUCCESS;
}

EngineResult
VulkanEngine::render_text_geometry(VkCommandBuffer cmd) {
	// Write text data to buffer
	void* data = (void*)_mainDrawContext._textData.vertices.data();
	size_t size = sizeof(TextVertex) * _mainDrawContext._textData.vertices.size();

	CopyDataToBufferInfo copyInfo;
	copyInfo.buffer = textVertexBuffer.buffer;
	copyInfo.allocator = allocator;
	copyInfo.pData = data;
	copyInfo.size = size;
	copyInfo.device = device;
	copyInfo.pDeviceDispatch = &deviceDispatch;
	copyInfo.cmdBuf = immCmdBuf;
	copyInfo.cmdFence = immFence;
	copyInfo.queue = graphicsQueue;
	copy_data_to_buffer(&copyInfo);

	data = (void*)_mainDrawContext._textData.indices.data();
	size = sizeof(uint32_t) * _mainDrawContext._textData.indices.size();

	copyInfo.buffer = textIndexBuffer.buffer;
	copyInfo.pData = data;
	copyInfo.size = size;
	copy_data_to_buffer(&copyInfo);

	VkRenderingAttachmentInfo colorAttachment = {};
	colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	colorAttachment.pNext = NULL;

	colorAttachment.imageView = drawImage.imageView;
	colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	VkRenderingInfo renderInfo = { .sType = VK_STRUCTURE_TYPE_RENDERING_INFO };
	renderInfo.pNext = nullptr;

	renderInfo.renderArea = VkRect2D{ VkOffset2D {0, 0}, drawExtent };
	renderInfo.layerCount = 1;
	renderInfo.colorAttachmentCount = 1;
	renderInfo.pColorAttachments = &colorAttachment;
	renderInfo.pDepthAttachment = nullptr;
	renderInfo.pStencilAttachment = nullptr;

	Pipeline p = pipelines[textPipeline];
	deviceDispatch.vkCmdBeginRendering(cmd, &renderInfo);

	deviceDispatch.vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
		p.pipeline);

	deviceDispatch.vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
		p.layout, 0, 1, &bindlessDescriptorSet, 0, nullptr);

	glm::mat4 view = glm::mat4(1.0f);
	glm::mat4 proj = glm::ortho(0.0f, (float)drawExtent.width / renderScale, 0.0f,
		(float)drawExtent.height / renderScale, -1.0f, 1.0f);

	GPUDrawPushConstants pc;
	pc.model = glm::mat4(0.0f);
	pc.vertexBuffer = textVertexBufferAddr;
	pc.sceneBuffer = uSceneDataAddr;

	deviceDispatch.vkCmdBindIndexBuffer(cmd, textIndexBuffer.buffer, 0,
		VK_INDEX_TYPE_UINT32);

	deviceDispatch.vkCmdPushConstants(cmd, pipelines[textPipeline].layout,
		VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &pc);

	deviceDispatch.vkCmdDrawIndexed(cmd, _mainDrawContext._textData.indices.size(),
		1, 0, 0, 0);

	deviceDispatch.vkCmdEndRendering(cmd);

	return ENGINE_SUCCESS;
}

/*  ============================================================================
 *  PUBLIC INTERFACE FUNCTIONS
 *  ============================================================================
 *
 *  These are funtions that are designed to be invoked
 *  outside of the VulkanEngine class for things like recording
 *  objects to be drawn, setting render variables, etc.
 */

// Set active camera for the scene.
void
VulkanEngine::set_active_camera(Camera c) {
	_activeCamera = c;
}

// Set the debug flags for rendering debug information
void
VulkanEngine::set_debug_flags(DebugFlags flags) {
	_debugFlags |= flags;
}

// Clear debug flags
void
VulkanEngine::clear_debug_flags(DebugFlags flags) {
	_debugFlags &= ~flags;
}

// Upload a mesh to the VulkanEngine (also uploads to gBuf)
void 
VulkanEngine::upload_mesh(std::span<uint32_t> indices, std::span<Vertex> vertices,
	GeoSurface* surfaces, uint32_t surfaceCount, const char* meshName) {

	MeshAsset newMesh = {};
	newMesh.name = meshName;

	const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
	const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

	for (size_t i = 0; i < surfaceCount; i++) {
		surfaces[i].materialID = 0;
		newMesh.surfaces.push_back(surfaces[i]);
	}

	newMesh.vertexOffset = gBuf.suballocate(vertexBufferSize, 8);
	newMesh.indexOffset = gBuf.suballocate(indexBufferSize, 8);

	AllocatedBuffer staging;
	BufferCreateInfo bufferInfo;
	bufferInfo.allocator = allocator;
	bufferInfo.pBuffer = &staging;
	bufferInfo.allocSize = vertexBufferSize + indexBufferSize;
	bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	bufferInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
		VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
	create_buffer(&bufferInfo);

	void* data = staging.info.pMappedData;

	// copy vertex buffer
	memcpy(data, vertices.data(), vertexBufferSize);
	// copy index buffer
	memcpy((char*)data + vertexBufferSize, indices.data(), indexBufferSize);

	immediate_submit([&](VkCommandBuffer cmd) {
		VkBufferCopy vertexCopy{ 0 };
		vertexCopy.dstOffset = newMesh.vertexOffset;
		vertexCopy.srcOffset = 0;
		vertexCopy.size = vertexBufferSize;

		deviceDispatch.vkCmdCopyBuffer(cmd, staging.buffer, gBuf.buffer, 1, &vertexCopy);

		VkBufferCopy indexCopy{ 0 };
		indexCopy.dstOffset = newMesh.indexOffset;
		indexCopy.srcOffset = vertexBufferSize;
		indexCopy.size = indexBufferSize;

		deviceDispatch.vkCmdCopyBuffer(cmd, staging.buffer, gBuf.buffer, 1, &indexCopy);
		});

	destroy_buffer(&staging);

	meshes[meshCount++] = newMesh;
}

// Adds the line to the main draw context
void
VulkanEngine::draw_line(glm::vec3 from, glm::vec3 to, glm::vec4 color) {
	_mainDrawContext.add_line(from, to, color);
}

// Adds trianlge vertices to the main draw context (does not support indexed drawing rn)
// @TODO ->	Indexed drawing please
void
VulkanEngine::draw_triangle(glm::vec3 vertices[3], glm::vec4 color) {
	_mainDrawContext.add_triangle(vertices, color);
}

// Adds the mesh with 'id' and transform data 'transform' to the
// main draw context (to be drawn in this frame).
void
VulkanEngine::draw_mesh(uint32_t id, const Transform* transform) {
	_mainDrawContext.add_mesh(&meshes[id], transform);
}

void
VulkanEngine::draw_wireframe(std::vector<glm::vec3>& vertices,
	std::vector<uint32_t>& indices) {
	// NOT IMPLEMENTED IN VK_CONTEXT.CPP
}

// Adds 'text' at position 'x', 'y' to the main draw context.
// @TODO -> Right now, only renders with defaultFont... allow
//			passing a fontAtlas id for custom fonts.
void
VulkanEngine::draw_text(const char* text, float x, float y, FontAtlas* pAtlas) {
	_mainDrawContext.add_text(text, { x, y, 1 }, &defaultFont);
}

// Creates a shader to be owned by the VulkanEngine.
EngineResult
VulkanEngine::create_shader(const char* path, EShLanguage stage, uint32_t* idx) {
	shaderMutex.lock();
	if (shaderCount >= MAX_SHADERS) {
		ENGINE_ERROR("Maximum shaders reached.");
		shaderMutex.unlock();
		return ENGINE_FAILURE;
	}

	if (load_shader_module(path, device, &shaders[shaderCount].shader,
		stage, &deviceDispatch) > 0) {
		shaderMutex.unlock();
		return ENGINE_FAILURE;
	}
	shaders[shaderCount].stage = stage;
	shaders[shaderCount].lastWrite = std::filesystem::last_write_time(path);
	shaders[shaderCount].path = path;

	*idx = shaderCount;

	shaderCount++;
	shaderMutex.unlock();

	return ENGINE_SUCCESS;
}

// Creates a material to be owned by the VulkanEngine, passing the material
// index via 'idx'.
EngineResult
VulkanEngine::create_material(const Material* material,
	uint32_t* idx) {
	if (materialCount >= MAX_MATERIALS) {
		ENGINE_ERROR("Failed to create material... max materials reached.");
		return ENGINE_FAILURE;
	}
	memcpy((Material*)uMaterialBuffer.info.pMappedData + materialCount++,
		material, sizeof(Material));

	return ENGINE_SUCCESS;
}