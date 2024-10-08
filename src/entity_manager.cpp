#include "entity_manager.h"

#include <stdio.h>

void
EntityManager::init(PhysicsContext* pPhysicsContext) {
	_pPhysicsContext = pPhysicsContext;
}

entity_t 
EntityManager::create_entity() {
	if (_entitiesCount >= MAX_ENTITIES) {
		fprintf(stderr, "[EntityManager] Failed to create entity: max reached.\n");
		return -1;
	}

	return _entitiesCount++;
}

void
EntityManager::add_transform(entity_t entity, const Transform* transform) {
	_transforms[entity] = *transform;
	_componentMasks[entity].set(TRANSFORM);
}

void
EntityManager::remove_transform(entity_t entity) {
	if (!has_component(entity, TRANSFORM)) {
		fprintf(stderr,
			"[EntityManager] ERROR: Attempting to remove transform when entity does not have one.\n");
		return;
	}
	_transforms[entity] = {};
	_componentMasks[entity].reset(TRANSFORM);
}

void
EntityManager::add_mesh(entity_t entity, uint32_t meshID) {
	_meshIDs[entity] = meshID;
	_componentMasks[entity].set(MESH);
}

void
EntityManager::remove_mesh(entity_t entity) {
	if (!has_component(entity, MESH)) {
		fprintf(stderr,
			"[EntityManager] ERROR: Attempting to remove mesh when entity does not have one.\n");
		return;
	}
	_meshIDs[entity] = {};
	_componentMasks[entity].reset(MESH);
}

void
EntityManager::add_physics_body(entity_t entity, glm::vec3 extent) {
	if (!has_component(entity, TRANSFORM)) {
		fprintf(stderr,
			"[EntityManager] ERROR: Entity must have transform in order to add a physics body.\n");
		return;
	}
	if (_pPhysicsContext == nullptr) {
		fprintf(stderr,
			"[EntityManager] ERROR: _pPhysicsSystem is nullptr.\n");
		return;
	}
	JPH::BodyID bodyID = _pPhysicsContext->add_box(_transforms[entity],
		JPH::Vec3(extent.x, extent.y, extent.z));
	_bodyIDs[entity] = bodyID;
	_componentMasks->set(PHYSICS_BODY);
}

void
EntityManager::remove_physics_body(entity_t entity) {
	if (!has_component(entity, PHYSICS_BODY)) {
		fprintf(stderr,
			"[EntityManager] ERROR: Attempting to remove physics body when entity does not have one.\n");
		return;
	}
	_bodyIDs[entity] = {};
	_componentMasks->reset(PHYSICS_BODY);
}

// Holy dear fucking lord this is slow as fuck
void
EntityManager::system_physics_update(float dt) {
	_pPhysicsContext->update(dt);

	JPH::PhysicsSystem* pPhysicsSystem = _pPhysicsContext->get_physics_system();
	JPH::BodyInterface& bodyInterface = pPhysicsSystem->GetBodyInterface();

	for (size_t e = 0; e < _entitiesCount; e++) {
		if (has_component(e, TRANSFORM) &&
			has_component(e, PHYSICS_BODY)) {

			Transform* transform = &_transforms[e];
			JPH::BodyID bodyID = _bodyIDs[e];;

			JPH::Vec3 position = bodyInterface.GetPosition(bodyID);
			JPH::Quat rotation = bodyInterface.GetRotation(bodyID);

			transform->position = glm::vec3{
				position.GetX(),
				position.GetY(),
				position.GetZ()
			};
			transform->rotation = glm::quat{
				rotation.GetW(),
				rotation.GetX(),
				rotation.GetY(),
				rotation.GetZ()
			};
		}
	}
}

void 
EntityManager::system_render_update(VulkanEngine* vk) {
	for (int e = 0; e < _entitiesCount; e++) {
		if (has_component(e, TRANSFORM) &&
			has_component(e, MESH)) {
			Transform* transform = &_transforms[e];
			uint32_t meshId = _meshIDs[e];

			vk->draw_mesh(meshId, transform);
		}
	}
}