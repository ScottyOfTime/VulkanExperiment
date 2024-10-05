#ifndef PHY_COLLISIONS_H
#define PHY_COLLISIONS_H

#include "phy_types.h"

#include <glm/glm.hpp>

uint32_t point_box_intersect(glm::vec3 p, CollisionBox a);
uint32_t box_intersect(CollisionBox a, CollisionBox b);

#endif /* PHY_COLLISIONS_H */