#include <stdio.h>

#include "SDL.h"
#include "vk_engine.h"
#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#include "backends/imgui_impl_sdl2.h"

int main(int argc, char* argv[])
{
	int quit;
	printf("[Main] Running vulkan demo.\n");
	
	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		fprintf(stderr, "[Main] Error initializing SDL: %s\n", SDL_GetError());
		return 1;
	}
	SDL_Window *win = SDL_CreateWindow("Vulkan Demo", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
			1280, 720, SDL_WINDOW_VULKAN);
	if (win == NULL) {
		fprintf(stderr, "[Main] SDL Window creation failed: %s\n", SDL_GetError());
	}

	VulkanEngine *vk = new VulkanEngine;
	if (vk->init(win) != ENGINE_SUCCESS) {
		fprintf(stderr, "[Main] Vulkan engine creation failed.\n");
		goto cleanup;
	}
	
	quit = 0;
	SDL_Event e;
	while (!quit) {
		while (SDL_PollEvent(&e)) {
			switch (e.type) {
				case SDL_QUIT:
					quit = 1;
			}

			ImGui_ImplSDL2_ProcessEvent(&e);
		}

		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

		ImGui::ShowDemoWindow();

		ImGui::Render();

		vk->draw();
	}

cleanup:
	vk->deinit();
	delete vk;
	SDL_DestroyWindow(win);
	return 0;
}
