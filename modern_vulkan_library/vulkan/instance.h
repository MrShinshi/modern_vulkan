#pragma once
#include "common/modern_vulkan_library.h"
#include "vulkan/extensions.h"
#include "vulkan/physical_device.h"

namespace modern_vulkan
{
namespace sdl
{
struct window;
}

struct instance_private;
struct surface;
struct MODERN_VULKAN_LIBRARY_EXPORT instance {
	instance(std::span<instance_extension const> extensions, std::span<instance_layer const> layers);
	~instance();

	instance(instance&& other) noexcept;
	instance& operator=(instance&& other) noexcept;

	instance(instance const&) = delete;
	instance& operator=(instance const&) = delete;

	auto handle() const -> void *;

	auto make_surface(sdl::window const&) -> surface const&;
	auto physical_devices() const -> std::vector<physical_device>;

private:
	instance_private *d;
};
} // namespace modern_vulkan