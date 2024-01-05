add_executable(vulkan main.cpp)

add_library(vulkanengine vk_engine.cpp)

if (WIN32)
	target_include_directories(vulkan PUBLIC
		C:/VulkanSDK/${VULKAN_SDK_VERSION}/Include
	)
	target_include_directories(vulkanengine PUBLIC
		C:/VulkanSDK/${VULKAN_SDK_VERSION}/Include
	)
endif()

target_link_libraries(vulkan vulkanengine)
