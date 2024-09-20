#ifndef _GAME_H
#define _GAME_H

#include "renderer/vk_engine.h"

enum Mode {
	PLAY,
	EDIT
};

struct Game {
	Mode mode = PLAY;
	uint8_t quit = 0;

	VulkanEngine* vk;

	void run();
	void cleanup();
};

#endif /* GAME_H */
