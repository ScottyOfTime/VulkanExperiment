#include "game.h"

#include "physics/phy_util.h"

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
	if (vk.init() != ENGINE_SUCCESS) {
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
	entityManager.add_component(lightSphere, COLLISION_BOX);
	entityManager.collisionBoxes[lightSphere] = create_collision_box(
		glm::vec3(6, 3, 2), 1, 1, 1
	);

	SDL_Event e;
	uint32_t prevTime = 0;
	uint32_t currentTime = 0;
	Camera camera = {};
	float dt = 0.0f;
	while (!quit) {
		currentTime = SDL_GetTicks();
		deltaTime = (currentTime - prevTime) / 1000.f;
		prevTime = currentTime;
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

		process_input();
		vk.set_active_camera(mainCamera);

		system_movement(&entityManager);
		system_collision(&entityManager, &vk);
		system_render(&entityManager, &vk);

		if (vk.resizeRequested) {
			if (vk.resize_swapchain() != ENGINE_SUCCESS) {
				fprintf(stderr, "[Main] Failed to resize swapchain.\n");
				quit = true;
			}
		}

		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();

		if (ImGui::Begin("background")) {
			ImGui::SliderFloat("Render Scale", &vk.renderScale, 0.3f, 1.f);
			ImGui::Text("Mode: %s", mode ? "EDIT" : "PLAY");
		}
		ImGui::End();

		ImGui::Render();

		vk.draw_text("SCOTTY'S ENGINE IN PROGRESS", 16, 32);
		if (mode == EDIT) {
			vk.draw_text("EDIT MODE", 24, 64);
			vk.set_debug(1);
		}
		else {
			vk.draw_text("PLAY MODE", 24, 64);
			vk.set_debug(0);
		}
		// render demo wireframe
		vk.draw();
	}
}

void Game::process_input() {
	const SDL_bool* keyStates = SDL_GetKeyboardState(NULL);
	switch (mode) {
	case PLAY:
		SDL_SetWindowRelativeMouseMode(vk.window, SDL_TRUE);
		camera_movement(keyStates);
		break;
	case EDIT:
		float x = 0, y = 0;
		SDL_MouseButtonFlags m;
		m = SDL_GetMouseState(&x, &y);
		if (m & SDL_BUTTON_RMASK) {
			SDL_SetWindowRelativeMouseMode(vk.window, SDL_TRUE);
			camera_movement(keyStates);
		} else {
			SDL_SetWindowRelativeMouseMode(vk.window, SDL_FALSE);
		}
		break;
	}
}

void Game::camera_movement(const SDL_bool* keyStates) {
	if (keyStates[SDL_SCANCODE_W]) {
		mainCamera.vel.z = -10 * deltaTime;
	}
	if (keyStates[SDL_SCANCODE_A]) {
		mainCamera.vel.x = -10 * deltaTime;
	}
	if (keyStates[SDL_SCANCODE_S]) {
		mainCamera.vel.z = 10 * deltaTime;
	}
	if (keyStates[SDL_SCANCODE_D]) {
		mainCamera.vel.x = 10 * deltaTime;
	}

	float x = 0, y = 0;
	SDL_GetRelativeMouseState(&x, &y);
	mainCamera.yaw += x / 500.f;
	mainCamera.pitch -= y / 500.f;

	mainCamera.pos += glm::vec3(mainCamera.calcRotationMat() *
		glm::vec4(mainCamera.vel * 0.5f, 0.f));
	mainCamera.vel = { 0, 0, 0 };
}

void Game::cleanup() {
	vk.deinit();
}
