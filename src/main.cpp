#include <stdio.h>

#include "SDL3/SDL.h"
#include "vk_engine.h"
#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#include "backends/imgui_impl_sdl3.h"

enum Mode {
	PLAY,
	EDIT
};

int main(int argc, char* argv[])
{
	uint8_t quit;
	Mode mode = PLAY;
	printf("[Main] Running vulkan demo.\n");
	
	if (SDL_Init(SDL_INIT_VIDEO) != SDL_TRUE) {
		fprintf(stderr, "[Main] Error initializing SDL: %s\n", SDL_GetError());
		return 1;
	}
	SDL_Window *win = SDL_CreateWindow("Vulkan Demo", 1280, 720, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
	if (win == NULL) {
		fprintf(stderr, "[Main] SDL Window creation failed: %s\n", SDL_GetError());
	}

	VulkanEngine *vk = new VulkanEngine;
	if (vk->init(win) != ENGINE_SUCCESS) {
		fprintf(stderr, "[Main] Vulkan engine creation failed.\n");
		goto cleanup;
	}

	//SDL_SetWindowRelativeMouseMode(win, SDL_TRUE);
	
	quit = 0;
	SDL_Event e;
	while (!quit) {
		switch (mode) {
			case PLAY:
				SDL_SetWindowRelativeMouseMode(win, SDL_TRUE);
				break;
			case EDIT:
				SDL_SetWindowRelativeMouseMode(win, SDL_FALSE);
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

		float x, y;
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

			ComputeEffect& selected = vk->backgroundEffects[vk->currentBackgroundEffect];

			ImGui::Text("Selected effect: ", selected.name);

			ImGui::SliderInt("Effect Index", &vk->currentBackgroundEffect, 0, vk->backgroundEffects.size() - 1);

			ImGui::InputFloat4("data1", (float*)&selected.data.data1);
			ImGui::InputFloat4("data2", (float*)&selected.data.data2);
			ImGui::InputFloat4("data3", (float*)&selected.data.data3);
			ImGui::InputFloat4("data4", (float*)&selected.data.data4);
		}
		ImGui::End();

		ImGui::Render();

		vk->draw();
	}

cleanup:
	vk->deinit();
	delete vk;
	SDL_DestroyWindow(win);
	return 0;
}
