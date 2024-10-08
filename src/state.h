#ifndef _STATE_H
#define _STATE_H

#include <SDL3/SDL.h>

#include "renderer/vk_engine.h"

constexpr uint32_t NUM_GAME_STATES = 3;

// Forward declaration
class Game;

typedef enum {
	PLAY,
	EDIT,
	MAIN_MENU
} GameState;

typedef struct {
	GameState state;
	void (*fpInputRoutine)(Game* pGame, const bool* pKeyState);
	void (*fpRenderRoutine)(Game* pGame);
	void (*fpStartRoutine)(Game* pGame);
} GameStateNode;

void play_input(Game* pGame, const bool* pKeyState);
void play_render(Game* pGame);
void play_start(Game* pGame);

void edit_input(Game* pGame, const bool* pKeyState);
void edit_render(Game* pGame);
void edit_start(Game* pGame);


#endif /* _SHATE_H */