#ifndef _GAME_H
#define _GAME_H

#include "renderer/vk_engine.h"
#include "entity_manager.h"
#include "physics/phys_main.h"
#include "state.h"

class Game {
public:
	void			init();
	void			deinit();

	void			run();

	void			transition_state(GameState newState);

	float			get_delta_time();

	void			move_camera(glm::vec3 velocity);
	void			rotate_camera(float pitch, float yaw);

	// The three horsemen (these may be directly accessed)
	VulkanEngine	vulkanEngine;
	EntityManager	entityManager;
	PhysicsContext	physicsContext;

private:
	GameState		_currentState = PLAY;
	GameStateNode	_states[NUM_GAME_STATES] = {
		{ PLAY, play_input, play_render, play_start },
		{ EDIT, edit_input, edit_render, edit_start },
		{}
	};

	uint8_t			_quit = 0;

	float			_deltaTime = 0;

	uint32_t		_physicsInitialized;

	Camera			_mainCamera = {
		.pos = { 0, 0, 0 }
	};
};

#endif /* GAME_H */
