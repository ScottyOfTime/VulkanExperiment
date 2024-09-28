#include "game.h"

#include "renderer/vk_engine.h"

void Game::run() {
	vk = new VulkanEngine;
	if (vk->init() != ENGINE_SUCCESS) {
		fprintf(stderr, "[Game] Vulkan engine creation failed.\n");
		return;
	}

	entity_t testCubes[32][32] = {};

	/*for (size_t i = 0; i < 32; i++) {
		for (size_t j = 0; j < 32; j++) {
			testCubes[i][j] = entityManager.create_entity();
			entityManager.add_component(testCubes[i][j], TRANSFORM);
			entityManager.transforms[testCubes[i][j]] = Transform{
				.position = glm::vec3(i * 4, j * 4, 0),
				.rotation = glm::quat(1, 0, 0, 0),
				.scale = glm::vec3(1, 1, 1)
			};
			entityManager.add_component(testCubes[i][j], MESH);
			entityManager.meshIds[testCubes[i][j]] = 0;
			entityManager.add_component(testCubes[i][j], VELOCITY);
			entityManager.velocities[testCubes[i][j]] = glm::vec3{
				0, 0, 0
			};
		}
	}*/

	entity_t testCube = entityManager.create_entity();
	entityManager.add_component(testCube, TRANSFORM);
	entityManager.transforms[testCube] = Transform{
		.position = glm::vec3(0, 0, 0),
		.rotation = glm::quat(1, 0, 0, 0),
		.scale = glm::vec3(1, 1, 1)
	};
	entityManager.add_component(testCube, MESH);
	entityManager.meshIds[testCube] = 0;
	entityManager.add_component(testCube, VELOCITY);
	entityManager.velocities[testCube] = glm::vec3{
		0, 0, 0
	};

	SDL_Event e;
	uint32_t prevTime = 0;
	uint32_t currentTime = 0;
	float dt = 0.0f;
	while (!quit) {
		currentTime = SDL_GetTicks();
		dt = (currentTime - prevTime) / 1000.f;
		prevTime = currentTime;
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
		vk->reset_instances();
		system_render(&entityManager, vk);

		//> Camera stuff should really belong elsewhere but probably
		//> exit elsewhere but also not within renderer except for
		//> position

		const SDL_bool* keyStates = SDL_GetKeyboardState(NULL);
		if (keyStates[SDL_SCANCODE_W]) {
			vk->camera.vel.z = -10 * dt;
		}
		if (keyStates[SDL_SCANCODE_A]) {
			vk->camera.vel.x = -10 * dt;
		}
		if (keyStates[SDL_SCANCODE_S]) {
			vk->camera.vel.z = 10 * dt;
		}
		if (keyStates[SDL_SCANCODE_D]) {
			vk->camera.vel.x = 10 * dt;
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
