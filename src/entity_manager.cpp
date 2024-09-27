#include "entity_manager.h"

#include <stdio.h>

entity_t EntityManager::create_entity() {
	if (entitiesCount >= MAX_ENTITIES) {
		fprintf(stderr, "[EntityManager] Failed to create entity: max reached.\n");
		return -1;
	}

	return entitiesCount++;
}

void system_movement(EntityManager* entityManager) {
	for (int e = 0; e < entityManager->entitiesCount; e++) {
		// Yeah, this is like totally not optimal at all but very
		// easy to optimize just use a 32 bit int and do bit checks
		// instead.
		if (entityManager->has_component(e, TRANSFORM) &&
			entityManager->has_component(e, VELOCITY)) {
			glm::vec3* pos = &entityManager->transforms[e].position;
			glm::vec3* vel = &entityManager->velocities[e];
			*pos += *vel;
		}
	}
}

void system_render(EntityManager* entityManager, VulkanEngine* vk) {
	for (int e = 0; e < entityManager->entitiesCount; e++) {
		if (entityManager->has_component(e, TRANSFORM) &&
			entityManager->has_component(e, MESH)) {
			Transform* transform = &entityManager->transforms[e];
			uint32_t meshId = entityManager->meshIds[e];

			vk->draw_mesh(meshId, transform);
		}
	}
}
