#include <SDL3/SDL.h>

#include <stdexcept>
#include <string>
#include <utility>

#include <sdl/window.h>
#include <sdl/impl/window_private.h>
#include <sdl/impl/properties_private.h>

namespace modern_vulkan::sdl
{

window::window(window_properties const& properties) : d(new window_private()) {
	d->window = SDL_CreateWindowWithProperties(properties.handle());

	if (d->window == nullptr) {
		throw std::runtime_error("SDL_CreateWindow failed: " + std::string(SDL_GetError()));
	}
}
window::~window() {
	delete d;
}

window::window(window&& other) noexcept : d(other.d) {
	other.d = nullptr;
}

window& window::operator=(window&& other) noexcept {
	if (this != &other) {
		delete std::exchange(d, std::exchange(other.d, nullptr));
	}
	return *this;
}
auto window::handle() const -> void * {
	return d->window;
}

void window::title(std::string_view value) const {
	if (d == nullptr || d->window == nullptr) {
		return;
	}

	if (!SDL_SetWindowTitle(d->window, std::string(value).c_str())) {
		throw std::runtime_error("SDL_SetWindowTitle failed: " + std::string(SDL_GetError()));
	}
}
} // namespace modern_vulkan::sdl