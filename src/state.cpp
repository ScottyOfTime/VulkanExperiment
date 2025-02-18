#include "state.h"
#include "game.h"

void play_input(Game* pGame, const bool* pKeyState) {
	float dt = pGame->get_delta_time();

	float x = 0, y = 0;
	SDL_GetRelativeMouseState(&x, &y);
	float yaw = x / 500.f;
	float pitch = -(y / 500.f);

	pGame->entityManager.system_player_controller_update(pKeyState, yaw);
}

void play_render(Game* pGame) {
	VulkanEngine* vulkanEngine = &pGame->vulkanEngine;
	vulkanEngine->begin();
	// Why pass a pointer to vulkan engine for this? Surely just let the
	// entity manager hold a pointer to it
	Light testLight = {
		.position = glm::vec3(0.0f, 5.0f, 0.0f),
		.constant = 0.5,
		.direction = glm::normalize(glm::vec3(0.0f, -1.0f, 0.0)),
		.linear = 0.09,
		.color = glm::vec3(0.5f, 0.5f, 0.5f),
		.quadratic = {},
		.innerAngle = {},
		.outerAngle = {},
		.intensity = 0.8f,
		.type = LightType::Direction
	};
	vulkanEngine->add_light(&testLight);
	pGame->entityManager.system_render_update(vulkanEngine);
	vulkanEngine->draw_text("Testing text", 64, 64, &vulkanEngine->defaultFont);
	vulkanEngine->draw();
}

void play_start(Game* pGame) {
	SDL_SetWindowRelativeMouseMode(pGame->vulkanEngine.window, true);
	SDL_GetRelativeMouseState(NULL, NULL);
	pGame->vulkanEngine.clear_debug_flags(RENDER_DEBUG_ENABLE_BIT);
	fprintf(stderr, "Transitioning to play state.\n");
}



void edit_input(Game* pGame, const bool* pKeyState) {
	float x = 0, y = 0;
	SDL_MouseButtonFlags m;
	m = SDL_GetMouseState(&x, &y);
	if (m & SDL_BUTTON_RMASK) {
		SDL_SetWindowRelativeMouseMode(pGame->vulkanEngine.window, true);
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
		float yaw = -(x / 500.f);
		float pitch = -(y / 500.f);

		pGame->_editCamera.rotate(pitch, yaw);
		pGame->_editCamera.move(cameraVel);
	}
	else {
		SDL_SetWindowRelativeMouseMode(pGame->vulkanEngine.window, false);
	}
}

void edit_render(Game* pGame) {
	VulkanEngine* vulkanEngine = &pGame->vulkanEngine;
	vulkanEngine->begin();

	if (ImGui::Begin("Physics Debugging")) {
		ImGui::CheckboxFlags("Body Wireframe", &pGame->physicsContext._debugFlags, PHYSICS_DEBUG_BODY_WIREFRAME_BIT);
	}
	ImGui::End();

	vulkanEngine->set_active_camera(pGame->_editCamera);
	Light testLight = {
		.position = glm::vec3(0.0f, 5.0f, 0.0f),
		.constant = 0.5,
		.direction = glm::normalize(glm::vec3(0.5f, -1.0f, 0.0)),
		.linear = 0.09,
		.color = glm::vec3(0.5f, 0.5f, 0.5f),
		.quadratic = {},
		.innerAngle = {},
		.outerAngle = {},
		.intensity = 0.1f,
		.type = LightType::Direction
	};
	vulkanEngine->add_light(&testLight);
	/*testLight.direction = glm::vec3(1.0f, 0.0f, 0.0f);
	testLight.color = glm::vec3(0.5f, 0.0f, 0.0f);
	testLight.intensity = 0.8f;
	vulkanEngine->add_light(&testLight);*/
	pGame->entityManager.system_render_update(vulkanEngine);
	vulkanEngine->draw();
}

void edit_start(Game* pGame) {
	SDL_SetWindowRelativeMouseMode(pGame->vulkanEngine.window, false);
	pGame->vulkanEngine.set_debug_flags(RENDER_DEBUG_ENABLE_BIT);
	fprintf(stderr, "Transitioning to edit state.\n");
}