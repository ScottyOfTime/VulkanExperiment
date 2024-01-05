cmake_minimum_required(VERSION 3.20)

project(VulkanExperiment)

if (NOT USE_PLATFORM MATCHES "VK_USE_PLATFORM_.*")
	if (WIN32)
		set(USE_PLATFORM "VK_USE_PLATFORM_WIN32_KHR")
	elseif(UNIX)
		set(USE_PLATFORM "VK_USE_PLATFORM_XCB_KHR")
	endif()
endif()



set(VULKAN_SDK_VERSION 1.3.268.0)

add_definitions(-D${USE_PLATFORM})

add_subdirectory(src)
