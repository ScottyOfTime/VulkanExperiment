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
		if (entityManager->has_component(e, POSITION) &&
			entityManager->has_component(e, VELOCITY)) {
			glm::vec3* pos = &entityManager->positions[e];
			glm::vec3* vel = &entityManager->velocities[e];
			*pos += *vel;
		}
	}
}
