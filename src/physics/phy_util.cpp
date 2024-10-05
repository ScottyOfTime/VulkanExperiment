#include "phy_util.h"

CollisionBox create_collision_box(glm::vec3 p, float xLen, float yLen, float zLen) {
    CollisionBox b;
    b.minX = p.x;
    b.maxX = p.x + xLen;
    b.minY = p.y;
    b.maxY = p.y + yLen;
    b.minZ = p.z;
    b.maxZ = p.z + zLen;
    return b;
}

void gen_collision_box_wireframe(const CollisionBox* b, std::vector<glm::vec3>& vertices,
	std::vector<uint32_t>& indices) {
    glm::vec3 v0(b->minX, b->minY, b->minZ);
    glm::vec3 v1(b->maxX, b->minY, b->minZ);
    glm::vec3 v2(b->maxX, b->maxY, b->minZ);
    glm::vec3 v3(b->minX, b->maxY, b->minZ);
    glm::vec3 v4(b->minX, b->minY, b->maxZ);
    glm::vec3 v5(b->maxX, b->minY, b->maxZ);
    glm::vec3 v6(b->maxX, b->maxY, b->maxZ);
    glm::vec3 v7(b->minX, b->maxY, b->maxZ);

    vertices = {
        v0, v1, v2, v3, v4, v5, v6, v7
    };

    indices = {
        0, 1, 2,  2, 3, 0,
        4, 7, 6,  6, 5, 4,
        0, 3, 7,  7, 4, 0,
        1, 5, 6,  6, 2, 1,
        3, 2, 6,  6, 7, 3,
        0, 4, 5,  5, 1, 0
    };
}