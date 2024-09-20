#ifndef _TIMER_H
#define _TIMER_H

#include <SDL3/SDL.h>

struct Timer {
	uint32_t start_ticks = 0;
	uint32_t pause_ticks = 0;
	uint8_t on = 0;
	uint8_t paused = 0;

	void start() {
		start_ticks = SDL_GetTicks();
		on = 1;
		paused = 0;
	}

	uint32_t stop() {
		if (on) {
			on = 0;
			paused = 0;
			return SDL_GetTicks() - start_ticks;
		}
		else {
			fprintf(stderr, "[Timer] Not started.\n");
			return 0;
		}
	}

	uint32_t get_ticks() {
		if (on) {
			if (paused) {
				return pause_ticks;
			}
			else {
				return SDL_GetTicks() - start_ticks;
			}
		}
	}

	void pause() {
		if (on && !paused) {
			paused = 1;
			pause_ticks = SDL_GetTicks() - start_ticks;
		}
		else if (on && paused) {
			paused = 0;
			start_ticks = SDL_GetTicks() - pause_ticks;
		}
		else {
			fprintf(stderr, "[Timer] Not started.\n");
		}
	}
};

#endif /* _TIMER_H */
