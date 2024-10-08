# Build the renderer library first, obviously
add_subdirectory(renderer) # Builds the vulkanengine library

find_library(JOLT_LIBRARY Jolt PATHS "C:/devel/JoltPhysics/Build/VS2022_CL/Debug")

set(PHYSICS_SRC_FILES
	physics/phys_main.cpp
	physics/phys_debug.cpp
)

set(GAME_SRC_FILES
	main.cpp
	game.cpp
	entity_manager.cpp
	state.cpp
)

add_executable(vulkan ${PHYSICS_SRC_FILES} ${GAME_SRC_FILES})

target_compile_definitions(vulkan PUBLIC 
    JPH_FLOATING_POINT_EXCEPTIONS_ENABLED=1
    JPH_PROFILE_ENABLED=1 
    JPH_DEBUG_RENDERER=1 
    JPH_OBJECT_STREAM=1
	JPH_DEBUG_RENDERER=1
)

if (WIN32)
	target_include_directories(vulkan PUBLIC
		C:/devel/SDL/include/
		C:/devel/JoltPhysics
		${VULKAN_SDK}/Include
		${PROJECT_SOURCE_DIR}/third-party/imgui
	)
	target_link_libraries(vulkan PRIVATE
		vulkanengine
	)
	if(JOLT_LIBRARY)
		target_link_libraries(vulkan PRIVATE ${JOLT_LIBRARY})
	else()
		message(FATAL_ERROR "Jolt library not found!")
endif()
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
		${SDL2_LIBRARIES}
	)
endif()

