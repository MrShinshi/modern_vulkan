#pragma once
#include <SDL3/SDL.h>

namespace modern_vulkan::sdl
{
struct window_private {
	~window_private() {
		SDL_DestroyWindow(window);
	}

	SDL_Window *window = nullptr;
};
} // namespace modern_vulkan::sdl