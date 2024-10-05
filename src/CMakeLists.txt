# Build the renderer library first, obviously
add_subdirectory(renderer) # Builds the vulkanengine library
add_subdirectory(physics)  # Builds the physics library

add_executable(vulkan main.cpp game.cpp entity_manager.cpp)

if (WIN32)
	target_include_directories(vulkan PRIVATE
		C:/devel/SDL/include/
		${VULKAN_SDK}/Include
		${PROJECT_SOURCE_DIR}/third-party/imgui
	)
	target_link_libraries(vulkan PRIVATE
		vulkanengine
		libphysics
	)
elseif(UNIX)
	# @Todo -> Been developing mostly on win recently so this is old
	find_package(SDL2 REQUIRED)
	target_include_directories(vulkan PUBLIC
		$ENV{VULKAN_SDK}/include
		$ENV{HOME}/SDL2/include
		${PROJECT_SOURCE_DIR}/third-party/imgui
	)
	target_link_libraries(vulkan
		vulkanengine
		libphysics
		${SDL2_LIBRARIES}
	)
endif()

