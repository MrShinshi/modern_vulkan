#include "window.h"
#include <SDL3/SDL.h>
#include <stdexcept>
#include "properties_private.h"

namespace modern_vulkan
{
struct window_private {
	SDL_Window *window;
};

window::window(window_properties properties) : d(new window_private()) {
	d->window = SDL_CreateWindowWithProperties(properties.d->props);

	if (d->window == nullptr) {
		throw std::runtime_error("SDL_CreateWindow failed: " + std::string(SDL_GetError()));
	}
}
window::~window() {
	SDL_DestroyWindow(d->window);
	delete d;
}

window::window(window&& other) noexcept : d(other.d) {
	other.d = nullptr;
}

window& window::operator=(window&& other) noexcept {
	delete std::exchange(d, std::exchange(other.d, nullptr));
	return *this;
}

window::window(window const& other)
	: d(other.d ? new window_private(*other.d) : nullptr) {
}

window& window::operator=(window const& other) {
	if (this != &other) {
#pragma warning(push)
#pragma warning(disable : 6011)
		delete std::exchange(d, other.d ? new window_private(*other.d) : nullptr);
#pragma warning(pop)
	}
	return *this;
}
} // namespace modern_vulkan