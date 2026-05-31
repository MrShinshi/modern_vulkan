#pragma once
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.hpp>

namespace modern_vulkan
{
struct surface {
	surface(vk::Instance& instance, SDL_Window *window);
	~surface();

	surface(surface const&) = delete;
	surface& operator=(surface const&) = delete;

	auto handle() const -> void *;

private:
	vk::SurfaceKHR surface_;
	vk::Instance& instance_;
};
} // namespace modern_vulkan
