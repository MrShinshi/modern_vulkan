#pragma once
#include <SDL3/SDL.h>

namespace modern_vulkan
{
struct window_properties_private {
	SDL_PropertiesID props = SDL_CreateProperties();
};
}