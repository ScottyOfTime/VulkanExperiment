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

	VulkanEngine* vk;
	EntityManager entityManager;

	void run();
	void cleanup();
};

#endif /* GAME_H */
