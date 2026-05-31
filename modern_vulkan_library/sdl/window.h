#pragma once
#include <common/modern_vulkan_library.h>
#include <sdl/properties.h>

#include <string_view>

namespace modern_vulkan
{
struct instance;
namespace sdl
{
struct window_private;
struct MODERN_VULKAN_LIBRARY_EXPORT window {
	window(window_properties const&);
	~window();

	window(window&& other) noexcept;
	window& operator=(window&& other) noexcept;
	window(window const&) = delete;
	window& operator=(window const&) = delete;

	auto handle() const -> void *;
	void title(std::string_view value) const;

private:
	window_private *d;
};
} // namespace sdl
} // namespace modern_vulkan