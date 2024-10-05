#ifndef PHY_UTIL_H
#define PHY_UTIL_H

#include "phy_types.h"

#include <vector>
#include <glm/glm.hpp>

CollisionBox create_collision_box(glm::vec3 p, float xLen, float yLen, float zLen);

void gen_collision_box_wireframe(const CollisionBox* b, std::vector<glm::vec3>& vertices,
	std::vector<uint32_t>& indices);

#endif /* PHY_UTIL_H */