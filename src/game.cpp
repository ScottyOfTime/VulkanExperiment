#include "game.h"

#include "renderer/vk_engine.h"

void Game::run() {
	vk = new VulkanEngine;
	if (vk->init() != ENGINE_SUCCESS) {
		fprintf(stderr, "[Game] Vulkan engine creation failed.\n");
		return;
	}

	entity_t testEntity = entityManager.create_entity();
	entityManager.add_component(testEntity, POSITION);
	entityManager.add_component(testEntity, VELOCITY);
	entityManager.positions[testEntity] = glm::vec3{ 0, 0, 0 };
	entityManager.velocities[testEntity] = glm::vec3{ 1, 1, 1 };

	SDL_Event e;
	while (!quit) {
		switch (mode) {
			case PLAY:
				SDL_SetWindowRelativeMouseMode(vk->window, SDL_TRUE);
				break;
			case EDIT:
				SDL_SetWindowRelativeMouseMode(vk->window, SDL_FALSE);
				break;
		}
		while (SDL_PollEvent(&e)) {
			switch (e.type) {
				case SDL_EVENT_QUIT:
					quit = 1;
					break;
				case SDL_EVENT_KEY_DOWN:
					switch (e.key.key) {
						case SDLK_ESCAPE:
							mode = mode == PLAY ? EDIT : PLAY;
					}
			}

			ImGui_ImplSDL3_ProcessEvent(&e);
		}

		system_movement(&entityManager);

		//> Camera stuff should really belong elsewhere but probably
		//> exit elsewhere but also not within renderer except for
		//> position

		const SDL_bool* keyStates = SDL_GetKeyboardState(NULL);
		if (keyStates[SDL_SCANCODE_W]) {
			vk->camera.vel.z = -1 / 100.f;
		}
		if (keyStates[SDL_SCANCODE_A]) {
			vk->camera.vel.x = -1 / 100.f;
		}
		if (keyStates[SDL_SCANCODE_S]) {
			vk->camera.vel.z = 1 / 100.f;
		}
		if (keyStates[SDL_SCANCODE_D]) {
			vk->camera.vel.x = 1 / 100.f;
		}

		float x = 0, y = 0;
		SDL_GetRelativeMouseState(&x, &y);
		if (mode == PLAY) {
			vk->camera.yaw += x / 500.f;
			vk->camera.pitch -= y / 500.f;

			vk->camera.pos += glm::vec3(vk->camera.calcRotationMat() * glm::vec4(vk->camera.vel * 0.5f, 0.f));
		}
		vk->camera.vel = { 0, 0, 0 };

		//> Camera stuff end

		if (vk->resizeRequested) {
			if (vk->resize_swapchain() != ENGINE_SUCCESS) {
				fprintf(stderr, "[Main] Failed to resize swapchain.\n");
				quit = true;
			}
		}

		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();

		if (ImGui::Begin("background")) {
			ImGui::SliderFloat("Render Scale", &vk->renderScale, 0.3f, 1.f);
			ImGui::Text("Mode: %s", mode ? "EDIT" : "PLAY");

			ImGui::InputFloat3("Ambient Color + Strength", (float*)&vk->sceneData.ambience);
			ImGui::SliderFloat("Ambient Strength", &vk->sceneData.ambience.a, 0, 1);
		}
		ImGui::End();

		ImGui::Render();

		vk->draw();
	}
}

void Game::cleanup() {
	vk->deinit();	
	delete vk;
}
