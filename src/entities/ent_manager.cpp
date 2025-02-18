#include "ent_manager.h"

#include <stdio.h>

/*
* This is the default player input handler, it probably belongs somewhere else
* but i've put it here for now.
*/
void
default_player_input_routine(JPH::Character* pCharacter, const bool* pKeyState,
	float relMouseX) {
	pCharacter->SetRotation(JPH::Quat::sRotation(JPH::Vec3::sAxisY(), relMouseX));
	glm::vec3 moveDir(0.0f);
	if (pKeyState[SDL_SCANCODE_W]) moveDir += glm::vec3(0.0f, 0.0f, 1.0f);
	if (pKeyState[SDL_SCANCODE_S]) moveDir += glm::vec3(0.0f, 0.0f, -1.0f);
	if (pKeyState[SDL_SCANCODE_A]) moveDir += glm::vec3(-1.0f, 0.0f, 0.0f);
	if (pKeyState[SDL_SCANCODE_D]) moveDir += glm::vec3(1.0f, 0.0f, 0.0f);

	pCharacter->SetLinearVelocity(JPH::Vec3(moveDir.x, moveDir.y, moveDir.z));
}

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

void
EntityManager::add_player_controller(entity_t entity, float radius, float height,
	glm::vec3 cameraPos) {
	if (!has_component(entity, TRANSFORM)) {
		fprintf(stderr,
			"[EntityManager] ERROR: Attempting to add player controller component to entity that does not have a transform\n");
		return;
	}
	Transform* transform = &_transforms[entity];
	JPH::Vec3 position(
		transform->position.x,
		transform->position.y,
		transform->position.z
	);
	PlayerController p;
	p.active = 1;
	p.pPhysicsCharacter = _pPhysicsContext->create_character(position, radius, height);
	p.camera = Camera{ cameraPos };
	_playerControllers[entity] = p;
	_componentMasks[entity].set(PLAYER_CONTROLLER);
}

void
EntityManager::remove_player_controller(entity_t entity) {
	if (!has_component(entity, PLAYER_CONTROLLER)) {
		fprintf(stderr,
			"[EntityManager] ERROR: Attempting to remove physics body when entity does not have one.\n");
		return;
	}
	_playerControllers[entity] = {};
	_componentMasks[entity].reset(PLAYER_CONTROLLER);
}

void
EntityManager::system_player_controller_update(const bool* pKeyState, float relMouseX) {
	for (size_t e = 0; e < _entitiesCount; e++) {
		if (has_component(e, PLAYER_CONTROLLER)) {
			PlayerController* controller = &_playerControllers[e];
			controller->fpPlayerInputRoutine(controller->pPhysicsCharacter,
				pKeyState, relMouseX);
			_pActicvePlayerController = controller;
		}
	}
}

// Holy dear fucking lord this is slow as fuck
void
EntityManager::system_physics_update(float dt) {
	_pPhysicsContext->update(dt);

	JPH::PhysicsSystem* pPhysicsSystem = _pPhysicsContext->get_physics_system();
	JPH::BodyInterface& bodyInterface = pPhysicsSystem->GetBodyInterface();

	// Sync entity's physics bodies with their transform
	for (size_t e = 0; e < _entitiesCount; e++) {
		if (has_component(e, TRANSFORM)) {
			if (has_component(e, PHYSICS_BODY)) {
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
			if (has_component(e, PLAYER_CONTROLLER)) {
				Transform* transform = &_transforms[e];
				PlayerController* controller = &_playerControllers[e];

				JPH::Vec3 position = controller->pPhysicsCharacter->GetPosition();
				JPH::Quat rotation = controller->pPhysicsCharacter->GetRotation();

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