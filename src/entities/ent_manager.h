#ifndef ENT_MANAGER_H
#define ENT_MANAGER_H

#include <cstdint>
#include <bitset>

#include "../renderer/vk_types.h"
#include "../renderer/vk_engine.h"

#include "../physics/phys_main.h"

#include "ent_components.h"

#define MAX_ENTITIES	1024
#define MAX_COMPONENTS	32

typedef uint32_t entity_t;

enum ComponentId : uint32_t {
	TRANSFORM = 0,
	CAMERA = 1,
	MESH = 2,
	PHYSICS_BODY = 3,
	PLAYER_CONTROLLER = 4
};

class EntityManager {
public:
	void init(PhysicsContext* pPhysicsContext);

	uint32_t has_component(entity_t entity, uint32_t id) {
		return _componentMasks[entity].test(id);
	}

	void add_component(entity_t entity, uint32_t id) {
		_componentMasks[entity].set(id);
	}

	void remove_component(entity_t entity, uint32_t id) {
		_componentMasks[entity].reset(id);
	}

	entity_t	create_entity();

	void		add_transform(entity_t entity, const Transform* transform);
	void		remove_transform(entity_t entity);

	void		add_mesh(entity_t entity, uint32_t meshID);
	void		remove_mesh(entity_t entity);

	void		add_physics_body(entity_t entity, glm::vec3 extent);
	void		remove_physics_body(entity_t entity);

	void		add_player_controller(entity_t entity, float radius, float height,
					glm::vec3 cameraPos);
	void		remove_player_controller(entity_t entity);


	void		system_player_controller_update(const bool* pKeyState, float relMouseX);
	void		system_physics_update(float dt);
	void		system_render_update(VulkanEngine* pVulkanEngine);

private:
	std::bitset<MAX_COMPONENTS> _componentMasks[MAX_ENTITIES] = {};
	uint32_t					_entitiesCount = 0;

	// There should only be one active player controller at a time
	// I currently do not check for this :)
	PlayerController*			_pActicvePlayerController;

	Transform					_transforms[MAX_ENTITIES] = {};
	uint32_t					_meshIDs[MAX_ENTITIES] = {};
	JPH::BodyID					_bodyIDs[MAX_ENTITIES] = {};
	PlayerController			_playerControllers[MAX_ENTITIES] = {};

	PhysicsContext*				_pPhysicsContext = nullptr;
};


#endif /* _ENTITY_H */
