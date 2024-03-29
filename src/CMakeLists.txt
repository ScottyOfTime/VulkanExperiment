#find_package(VulkanMemoryAllocator CONFIG REQUIRED)

add_executable(vulkan main.cpp)

add_library(vulkanengine 
	vk_engine.cpp vk_dispatch.cpp vk_images.cpp vk_descriptors.cpp
	vk_pipelines.cpp
)

if (WIN32)
	target_include_directories(vulkan PUBLIC
		C:/devel/SDL2-VC/include
		C:/VulkanSDK/${VULKAN_SDK_VERSION}/Include
		${PROJECT_SOURCE_DIR}/third-party/imgui
	)
target_include_directories(vulkanengine PRIVATE
		C:/devel/SDL2-VC/include
		C:/VulkanSDK/${VULKAN_SDK_VERSION}/Include
		${PROJECT_SOURCE_DIR}/third-party/imgui
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
	imgui
)


