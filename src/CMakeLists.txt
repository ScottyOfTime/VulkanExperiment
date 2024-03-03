#find_package(VulkanMemoryAllocator CONFIG REQUIRED)

add_executable(vulkan main.cpp)

add_library(vulkanengine 
	vk_engine.cpp vk_dispatch.cpp vk_images.cpp vk_descriptors.cpp
)

if (WIN32)
	target_include_directories(vulkan PUBLIC
		C:/devel/SDL2-VC/include
		C:/VulkanSDK/${VULKAN_SDK_VERSION}/Include
	)
target_include_directories(vulkanengine PRIVATE
		C:/devel/SDL2-VC/include
		C:/VulkanSDK/${VULKAN_SDK_VERSION}/Include
	)
endif()


target_link_libraries(vulkan 
	${CMAKE_SOURCE_DIR}/lib/SDL2.lib
	${CMAKE_SOURCE_DIR}/lib/SDL2main.lib
	${CMAKE_SOURCE_DIR}/lib/SDL2test.lib
	vulkanengine
)

target_link_libraries(vulkanengine
	GPUOpen::VulkanMemoryAllocator
)


