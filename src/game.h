#ifndef _GAME_H
#define _GAME_H

#include "renderer/vk_engine.h"
#include "entity_manager.h"

enum Mode {
	PLAY,
	EDIT
};

struct Game {
	Mode mode = PLAY;
	uint8_t quit = 0;

	VulkanEngine vk;
	EntityManager entityManager;

	Camera mainCamera = {};

	float deltaTime;

	void run();
	void cleanup();

	void process_input();
	void camera_movement(const SDL_bool* keyStates);
};

#endif /* GAME_H */
