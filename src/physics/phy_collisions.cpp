#include "phy_collisions.h"

uint32_t point_box_intersect(glm::vec3 p, CollisionBox b) {
    return (
        p.x >= b.minX &&
        p.x <= b.maxX &&
        p.y >= b.minY &&
        p.y <= b.maxY &&
        p.z >= b.minZ &&
        p.z <= b.maxZ
    );
}

uint32_t box_intersect(CollisionBox a, CollisionBox b) {
	return (
        a.minX <= b.maxX &&
        a.maxX >= b.minX &&
        a.minY <= b.maxY &&
        a.maxY >= b.minY &&
        a.minZ <= b.maxZ &&
        a.maxZ >= b.minZ
	);
}