#ifndef ENT_COMPONENTS_H
#define ENT_COMPONENTS_H

#include <stdint.h>

#include <Jolt/Physics/Character/Character.h>

typedef void (*PlayerInputRoutine)
	(JPH::Character* pCharacter, const bool* pKeyState, float relMouseX);

void default_player_input_routine(JPH::Character* pCharacter, const bool* pKeyState,
	float relMouseX);

/*
* The player controller is a pointer to Jolt Physics Character, a camera
* (defined as local coordinates relative to an entity's transform), a function
* pointer to the input routine of the controller (by default assumes a basic 
* wasd to move routine) and a flag to set whether it is active or not.
*/
struct PlayerController {
	JPH::Character*			pPhysicsCharacter;
	Camera					camera = {};

	PlayerInputRoutine		fpPlayerInputRoutine = default_player_input_routine;

	uint32_t				active = 0;
};

#endif /* ENT_COMPONENTS_H */