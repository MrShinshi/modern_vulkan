#include "simple_direct_media_layer.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <stdexcept>
#include <stop_token>
#include <iostream>
#include "vulkan_exception.h"

namespace modern_vulkan
{

struct simple_direct_media_layer_private {
	std::stop_source stop_source;
};

simple_direct_media_layer::simple_direct_media_layer()
	: d(new simple_direct_media_layer_private()) {

	if (not SDL_Init(SDL_INIT_VIDEO)) {
		throw sdl_exception("SDL_Init failed");
	}
}

simple_direct_media_layer::~simple_direct_media_layer() {
	SDL_Quit();
}

auto simple_direct_media_layer::window(
	window_properties const& properties) -> modern_vulkan::window {
	return {properties};
}

auto simple_direct_media_layer::extensions() const -> std::vector<std::string> {

	Uint32 extension_count;

	auto extensions_ptr = 
		SDL_Vulkan_GetInstanceExtensions(&extension_count);

	if (!extensions_ptr || extension_count == 0) {
		throw sdl_exception("SDL_Vulkan_GetInstanceExtensions failed");
	}

	return {extensions_ptr, extensions_ptr + extension_count};
}

auto simple_direct_media_layer::execute() -> int {

	while (!d->stop_source.stop_requested()) {
		SDL_Event event;

		while (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_EVENT_QUIT:
				d->stop_source.request_stop();
				break;

			default:
				// Do nothing.
				break;
			}
		}

		SDL_Delay(10);
	}

	return EXIT_SUCCESS;
}
} // namespace modern_vulkan
