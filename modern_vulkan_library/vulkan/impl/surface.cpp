#include "vulkan/surface.h"

#include <vulkan/vulkan.hpp>
#include <SDL3/SDL_vulkan.h>

namespace modern_vulkan
{
surface::surface(vk::Instance& instance, SDL_Window *window) : instance_(instance) {

	VkSurfaceKHR c_surface;
	if (!SDL_Vulkan_CreateSurface(
			window,
			static_cast<VkInstance>(instance),
			nullptr,
			&c_surface)) {
		throw std::runtime_error("SDL_Vulkan_CreateSurface failed");
	}

	surface_ = c_surface;
}

surface::~surface() {
	instance_.destroySurfaceKHR(surface_);
}
auto surface::handle() const -> void * {
	return static_cast<void *>(surface_);
}
} // namespace modern_vulkan