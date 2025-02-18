#include "game.h"

void
Game::init() {
	if (vulkanEngine.init() != ENGINE_SUCCESS) {
		fprintf(stderr, "[Game] Failed to initialize vulkan engine.\n");
		return;
	}
	physicsContext.init(&vulkanEngine);
	_physicsInitialized = 1;
	entityManager.init(&physicsContext);
	transition_state(_currentState);
}


void 
Game::deinit() {
	vulkanEngine.deinit();
	if (_physicsInitialized) {
		physicsContext.deinit();
	}
}

glm::vec3 cubePositions[] = {
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
	SDL_Event e;
	uint32_t prevTime = 0;
	uint32_t currentTime = 0;

	entity_t testCube = entityManager.create_entity();
	Transform transform = {
		.position = { 0, 0, 0 },
		.rotation = { 1, 0, 0, 0 },
		.scale = { 1, 1, 1 }
	};
	entityManager.add_transform(testCube, &transform);
	entityManager.add_mesh(testCube, 0);
	entityManager.add_physics_body(testCube, { 1, 1, 1 });

	entity_t testPlayer = entityManager.create_entity();
	transform = {
		.position { 0, 5, 0 },
		.rotation = { 1, 0, 0, 0},
		.scale = { 0.5, 0.5, 0.5 }
	};
	entityManager.add_transform(testPlayer, &transform);
	entityManager.add_mesh(testPlayer, 0);
	entityManager.add_player_controller(testPlayer, 1.0f, 1.0f,
		{ 0.f, 2.f, -1.f });

	while (!_quit) {
		currentTime = SDL_GetTicks();
		_deltaTime = (currentTime - prevTime) / 1000.f;
		prevTime = currentTime;
		while (SDL_PollEvent(&e)) {
			switch (e.type) {
			case SDL_EVENT_QUIT:
				_quit = 1;
				break;
			case SDL_EVENT_KEY_DOWN:
				switch (e.key.key) {
				case SDLK_ESCAPE:
					transition_state(_currentState == PLAY ? EDIT : PLAY);
				}
			}


			ImGui_ImplSDL3_ProcessEvent(&e);
		}

		const bool* keyState = SDL_GetKeyboardState(NULL);

		_states[_currentState].fpInputRoutine(this, keyState);

		entityManager.system_physics_update(_deltaTime);

		_states[_currentState].fpRenderRoutine(this);

		if (vulkanEngine.resizeRequested) {
			if (vulkanEngine.resize_swapchain() != ENGINE_SUCCESS) {
				fprintf(stderr, "[Main] Failed to resize swapchain.\n");
				_quit = true;
			}
		}
	}
}

void
Game::transition_state(GameState newState) {
	if (_states[newState].fpStartRoutine != nullptr) {
		_states[newState].fpStartRoutine(this);
	}
	_currentState = newState;
}

float
Game::get_delta_time() {
	return _deltaTime;
}