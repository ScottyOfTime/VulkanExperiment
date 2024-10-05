# Source files for physics
set(PHYSICS_SRC_FILES
	phy_util.cpp
	phy_collisions.cpp
)

add_library(libphysics ${PHYSICS_SRC_FILES})

# Find dependencies depending on OS
if (WIN32)
	# It irks me that the scope of this is PUBLIC but it won't work with PRIVATE for some reason...
	target_include_directories(libphysics PUBLIC
		${PROJECT_SOURCE_DIR}/third-party/glm
	)
elseif(UNIX)
	# @Todo -> Been developing mostly on win recently so this is old
	target_include_directories(libphysics PRIVATE
		${PROJECT_SOURCE_DIR}/third-party/glm
	)
endif()