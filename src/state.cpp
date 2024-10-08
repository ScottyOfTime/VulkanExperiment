#include "state.h"
#include "game.h"

void play_input(Game* pGame, const bool* pKeyState) {
	float dt = pGame->get_delta_time();
	glm::vec3 cameraVel = {};

	if (pKeyState[SDL_SCANCODE_W]) {
		cameraVel.z = -10 * dt;
	}
	if (pKeyState[SDL_SCANCODE_A]) {
		cameraVel.x = -10 * dt;
	}
	if (pKeyState[SDL_SCANCODE_S]) {
		cameraVel.z = 10 * dt;
	}
	if (pKeyState[SDL_SCANCODE_D]) {
		cameraVel.x = 10 * dt;
	}

	float x = 0, y = 0;
	SDL_GetRelativeMouseState(&x, &y);
	float yaw = x / 500.f;
	float pitch = -(y / 500.f);

	pGame->rotate_camera(pitch, yaw);
	pGame->move_camera(cameraVel);
}

void play_render(Game* pGame) {
	VulkanEngine* vulkanEngine = &pGame->vulkanEngine;
	vulkanEngine->begin();
	// Why pass a pointer to vulkan engine for this? Surely just let the
	// entity manager hold a pointer to it
	pGame->entityManager.system_render_update(vulkanEngine);
	vulkanEngine->draw_text("Tesing text", 64, 64, &vulkanEngine->defaultFont);
	vulkanEngine->draw();
}

void play_start(Game* pGame) {
	SDL_SetWindowRelativeMouseMode(pGame->vulkanEngine.window, SDL_TRUE);
	SDL_GetRelativeMouseState(NULL, NULL);
	pGame->vulkanEngine.clear_debug_flags(DEBUG_FLAGS_ENABLE);
	fprintf(stderr, "Transitioning to play state.\n");
}



void edit_input(Game* pGame, const bool* pKeyState) {
	float x = 0, y = 0;
	SDL_MouseButtonFlags m;
	m = SDL_GetMouseState(&x, &y);
	if (m & SDL_BUTTON_RMASK) {
		SDL_SetWindowRelativeMouseMode(pGame->vulkanEngine.window, SDL_TRUE);
		float dt = pGame->get_delta_time();
		glm::vec3 cameraVel = {};

		if (pKeyState[SDL_SCANCODE_W]) {
			cameraVel.z = -10 * dt;
		}
		if (pKeyState[SDL_SCANCODE_A]) {
			cameraVel.x = -10 * dt;
		}
		if (pKeyState[SDL_SCANCODE_S]) {
			cameraVel.z = 10 * dt;
		}
		if (pKeyState[SDL_SCANCODE_D]) {
			cameraVel.x = 10 * dt;
		}

		float x = 0, y = 0;
		SDL_GetRelativeMouseState(&x, &y);
		float yaw = x / 500.f;
		float pitch = -(y / 500.f);

		pGame->rotate_camera(pitch, yaw);
		pGame->move_camera(cameraVel);
	}
	else {
		SDL_SetWindowRelativeMouseMode(pGame->vulkanEngine.window, SDL_FALSE);
	}
}

void edit_render(Game* pGame) {
	VulkanEngine* vulkanEngine = &pGame->vulkanEngine;
	vulkanEngine->begin();
	pGame->entityManager.system_render_update(vulkanEngine);
	vulkanEngine->draw();
}

void edit_start(Game* pGame) {
	SDL_SetWindowRelativeMouseMode(pGame->vulkanEngine.window, SDL_FALSE);
	pGame->vulkanEngine.set_debug_flags(DEBUG_FLAGS_ENABLE);
	fprintf(stderr, "Transitioning to edit state.\n");
}