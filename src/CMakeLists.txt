# Build the renderer library first, obviously
add_subdirectory(renderer) # Builds the vulkanengine library

find_library(JOLT_LIBRARY Jolt PATHS "${PROJECT_SOURCE_DIR}/third-party/JoltPhysics/Build/VS2022_CL/Debug")

set(PHYSICS_SRC_FILES
	physics/phys_main.cpp
	physics/phys_debug.cpp
)

set(ENTITIES_SRC_FILES
	entities/ent_manager.cpp
)

set(GAME_SRC_FILES
	main.cpp
	game.cpp
	state.cpp
)

add_executable(vulkan ${PHYSICS_SRC_FILES} ${GAME_SRC_FILES} ${ENTITIES_SRC_FILES})

target_compile_definitions(vulkan PUBLIC 
    JPH_FLOATING_POINT_EXCEPTIONS_ENABLED=1
    JPH_PROFILE_ENABLED=1 
    JPH_DEBUG_RENDERER=1 
    JPH_OBJECT_STREAM=1
	JPH_DEBUG_RENDERER=1
	CMAKE_DISABLE_FIND_PACKAGE_SDL3=ON
)


if (WIN32)
	target_include_directories(vulkan PUBLIC
		${Vulkan_INCLUDE_DIRS}
		${PROJECT_SOURCE_DIR}/third-party/imgui
		${PROJECT_SOURCE_DIR}/third-party/JoltPhysics
	)
	target_link_libraries(vulkan PUBLIC
		vulkanengine
		SDL3::SDL3
		imgui
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

