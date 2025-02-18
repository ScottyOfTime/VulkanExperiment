# Vulkan renderer library
find_package(Vulkan REQUIRED)

# Source files for compilation
set(VULKAN_ENGINE_SRC_FILES
	vk_engine.cpp
	vk_dispatch.cpp
	vk_images.cpp
	vk_descriptors.cpp
	vk_pipelines.cpp
	vk_gltf.cpp
	vk_suballocator.cpp
	vk_text.cpp
	vk_buffers.cpp
	vk_context.cpp
)

link_directories(C:/VulkanSDK/${VULKAN_SDK_VERSION}/Lib/)

add_library(vulkanengine ${VULKAN_ENGINE_SRC_FILES})

# Find dependencies depending on OS
if (WIN32)
	# It irks me that the scope of this is PUBLIC but it won't work with PRIVATE for some reason...
	target_include_directories(vulkanengine PUBLIC
		${Vulkan_INCLUDE_DIRS}
		${PROJECT_SOURCE_DIR}/third-party/imgui
		${PROJECT_SOURCE_DIR}/third-party/glm
		${PROJECT_SOURCE_DIR}/third-party/tinygltf
		${PROJECT_SOURCE_DIR}/third-party/VulkanMemoryAllocator/include
		${PROJECT_SOURCE_DIR}/third-party/glslang
		${PROJECT_SOURCE_DIR}/third-party/stb_image
		${PROJECT_SOURCE_DIR}/third-party/stb_truetype
	)
elseif(UNIX)
	# @Todo -> Been developing mostly on win recently so this is old
	target_include_directories(vulkanengine PRIVATE
		$ENV{VULKAN_SDK}/include
		$ENV{HOME}/SDL2/include
		${PROJECT_SOURCE_DIR}/third-party/glm
		${PROJECT_SOURCE_DIR}/third-party/imgui
		${PROJECT_SOURCE_DIR}/third-party/tinygltf
	)
endif()

target_link_libraries(vulkanengine PRIVATE
	SDL3::SDL3
	GPUOpen::VulkanMemoryAllocator
	imgui
	#glslang::OSDependent -> This doesnt work for some reason
	glslang::glslang
	glslang::SPIRV
)
