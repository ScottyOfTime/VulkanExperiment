add_executable(vulkan main.cpp)

add_library(vulkanengine vk_engine.cpp vk_dispatch.cpp vk_images.cpp)

if (WIN32)
	target_include_directories(vulkan PUBLIC
		C:/devel/SDL2-VC/include
		C:/VulkanSDK/${VULKAN_SDK_VERSION}/Include
	)
	target_include_directories(vulkanengine PUBLIC
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

# Copy the required dlls to the exe directory ( BROKEN )
# add_custom_command(TARGET vulkan POST_BUILD
# 	COMMAND ${CMAKE_COMMAND} -E copy_if_different
# 	"${PROJECT_SOURCE_DIR}/libs/SDL2.dll"
# 	$<TARGET_FILE_DIR:vulkan>)
