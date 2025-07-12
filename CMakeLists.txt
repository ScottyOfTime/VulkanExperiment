cmake_minimum_required(VERSION 3.20)

find_package(Vulkan REQUIRED)

project(VulkanExperiment)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebug")

if (NOT USE_PLATFORM MATCHES "VK_USE_PLATFORM_.*")
	if (WIN32)
		set(USE_PLATFORM "VK_USE_PLATFORM_WIN32_KHR")
	elseif(UNIX)
		set(USE_PLATFORM "VK_USE_PLATFORM_XCB_KHR")
	endif()
endif()



set(VULKAN_SDK_VERSION 1.3.290.0)

add_definitions(-D${USE_PLATFORM})
add_compile_definitions(SDL_DISABLE_OLD_NAMES)

add_subdirectory(third-party)

add_subdirectory(src)
