#ifndef _ENTITY_H
#define _ENTITY_H

#include <cstdint>
#include <bitset>

#include "renderer/vk_types.h"
#include "renderer/vk_engine.h"
#include "physics/phy_types.h"

#define MAX_ENTITIES	1024
#define MAX_COMPONENTS	32

typedef uint32_t entity_t;


enum ComponentId : uint32_t {
	TRANSFORM = 0,
	VELOCITY = 1,
	MESH = 2,
	COLLISION_BOX = 3
};

class EntityManager {
public:
	uint32_t					entitiesCount = 0;

	std::bitset<MAX_COMPONENTS> componentMasks[MAX_ENTITIES] = {};

	Transform					transforms[MAX_ENTITIES] = {};
	glm::vec3					velocities[MAX_ENTITIES] = {};
	uint32_t					meshIds[MAX_ENTITIES] = {};
	CollisionBox				collisionBoxes[MAX_ENTITIES] = {};

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
};



void system_movement(EntityManager* entityManager);
void system_collision(EntityManager* entityManager, VulkanEngine* vk);
void system_render(EntityManager* entityManager, VulkanEngine* vk);

#endif /* _ENTITY_H */
