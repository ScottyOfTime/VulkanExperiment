#include "game.h"

glm::vec3 cubePositions[] = {
	glm::vec3(0.0f,   0.0f,   0.0f),
	glm::vec3(5.0f,  10.0f, -30.0f),    
	glm::vec3(-4.0f,  -6.0f,  -7.0f), 
	glm::vec3(-10.0f, -5.0f, -25.0f),    
	glm::vec3(7.0f,  -1.0f,  -9.0f),   
	glm::vec3(-5.0f,   8.0f, -18.0f),   
	glm::vec3(4.0f,  -5.0f,  -7.0f),  
	glm::vec3(4.0f,   5.0f,  -7.0f),   
	glm::vec3(4.0f,   0.5f,  -4.0f),    
	glm::vec3(-3.5f,  2.5f,  -4.0f)    
};

void Game::run() {
	vk = new VulkanEngine;
	if (vk->init() != ENGINE_SUCCESS) {
		fprintf(stderr, "[Game] Vulkan engine creation failed.\n");
		return;
	}


	for (int i = 0; i < 10; ++i) {
		entity_t sphere = entityManager.create_entity();

		// Add components
		entityManager.add_component(sphere, TRANSFORM);
		entityManager.add_component(sphere, MESH);
		entityManager.add_component(sphere, VELOCITY);

		glm::vec3 position = cubePositions[i]; // Offset from the initial sphere's position
		float angle = 20.f * i;
		glm::quat rotation = glm::angleAxis(glm::radians(angle), glm::vec3(0.0f, 1.0f, 0.0f)); // Rotate around Y-axis

		// Set components
		entityManager.transforms[sphere] = Transform{
			.position = position,
			.rotation = rotation,
			.scale = glm::vec3(1.f, 1.f, 1.f)
		};
		entityManager.meshIds[sphere] = 0;
		entityManager.velocities[sphere] = glm::vec3(0.0f, 0.0f, 0.0f);
	}

	entity_t lightSphere = entityManager.create_entity();
	entityManager.add_component(lightSphere, TRANSFORM);
	entityManager.transforms[lightSphere] = Transform{
		.position = glm::vec3(6, 3, 2),
		.rotation = glm::quat(1, 0, 0, 0),
		.scale = glm::vec3(0.5, 0.5, 0.5)
	};
	entityManager.add_component(lightSphere, MESH);
	entityManager.meshIds[lightSphere] = 1;
	entityManager.add_component(lightSphere, VELOCITY);
	entityManager.velocities[lightSphere] = glm::vec3{
		0, 0, 0
	};

	SDL_Event e;
	uint32_t prevTime = 0;
	uint32_t currentTime = 0;
	Camera camera = {};
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

		//system_movement(&entityManager);
		system_render(&entityManager, vk);

		//> Camera stuff should really belong elsewhere but probably
		//> exit elsewhere but also not within renderer except for
		//> position

		const SDL_bool* keyStates = SDL_GetKeyboardState(NULL);
		if (keyStates[SDL_SCANCODE_W]) {
			camera.vel.z = -10 * dt;
		}
		if (keyStates[SDL_SCANCODE_A]) {
			camera.vel.x = -10 * dt;
		}
		if (keyStates[SDL_SCANCODE_S]) {
			camera.vel.z = 10 * dt;
		}
		if (keyStates[SDL_SCANCODE_D]) {
			camera.vel.x = 10 * dt;
		}

		float x = 0, y = 0;
		SDL_GetRelativeMouseState(&x, &y);
		if (mode == PLAY) {
			camera.yaw += x / 500.f;
			camera.pitch -= y / 500.f;

			camera.pos += glm::vec3(camera.calcRotationMat() * glm::vec4(camera.vel * 0.5f, 0.f));
		}
		camera.vel = { 0, 0, 0 };

		vk->set_active_camera(camera);

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
		}
		ImGui::End();

		ImGui::Render();

		vk->draw_text("SCOTTY'S ENGINE IN PROGRESS", 16, 32);
		if (mode == EDIT) {
			vk->draw_text("EDIT MODE", 24, 64);
		}
		else {
			vk->draw_text("PLAY MODE", 24, 64);
		}
		vk->draw();
	}
}

void Game::cleanup() {
	vk->deinit();	
	delete vk;
}
