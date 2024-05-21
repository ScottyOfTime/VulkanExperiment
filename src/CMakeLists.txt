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
		${PROJECT_SOURCE_DIR}/third-party/glm
	)
	target_include_directories(vulkanengine PRIVATE
		C:/devel/SDL2-VC/include
		C:/VulkanSDK/${VULKAN_SDK_VERSION}/Include
		${PROJECT_SOURCE_DIR}/third-party/imgui
		${PROJECT_SOURCE_DIR}/third-party/glm
	)
	target_link_libraries(vulkan 
		${CMAKE_SOURCE_DIR}/lib/SDL2.lib
		${CMAKE_SOURCE_DIR}/lib/SDL2main.lib
		${CMAKE_SOURCE_DIR}/lib/SDL2test.lib
		vulkanengine
	)
elseif(UNIX)
	find_package(SDL2 REQUIRED)
	target_include_directories(vulkan PUBLIC
		$ENV{VULKAN_SDK}/include
		$ENV{HOME}/SDL2/include
		${PROJECT_SOURCE_DIR}/third-party/glm
		${PROJECT_SOURCE_DIR}/third-party/imgui
	)
	target_include_directories(vulkanengine PUBLIC
		$ENV{VULKAN_SDK}/include
		$ENV{HOME}/SDL2/include
		${PROJECT_SOURCE_DIR}/third-party/glm
		${PROJECT_SOURCE_DIR}/third-party/imgui
	)
	target_link_libraries(vulkan
		vulkanengine
		${SDL2_LIBRARIES}
	)
endif()



target_link_libraries(vulkanengine
	GPUOpen::VulkanMemoryAllocator
	imgui
)


