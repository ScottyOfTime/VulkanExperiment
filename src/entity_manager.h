#ifndef _ENTITY_H
#define _ENTITY_H

#include <cstdint>
#include <bitset>

#include <glm/glm.hpp>

#include "renderer/vk_types.h"

#define MAX_ENTITIES	1024
#define MAX_COMPONENTS	32

typedef uint32_t entity_t;

enum ComponentId : uint32_t {
	POSITION = 0,
	VELOCITY = 1,
};

class EntityManager {
public:
	entity_t					entities[MAX_ENTITIES] = {};
	uint32_t					entitiesCount = 0;

	std::bitset<MAX_COMPONENTS> componentMasks[MAX_ENTITIES] = {};

	glm::vec3					positions[MAX_ENTITIES] = {};
	glm::vec3					velocities[MAX_ENTITIES] = {};

	entity_t 					create_entity();
	void						destroy_entity();


	uint32_t has_component(entity_t entity, uint32_t id) {
		return componentMasks[entity].test(id);
	}

	void add_component(entity_t entity, uint32_t id) {
		componentMasks[entity].set(id);
	}

	void remove_component(entity_t entity, uint32_t id) {
		componentMasks[entity].reset(id);
	}

private:

};

void system_movement(EntityManager* entityManager);

#endif /* _ENTITY_H */
