#pragma once
#include "modern_vulkan_library.h"
#include "properties.h"

namespace modern_vulkan
{
struct window_private;
struct MODERN_VULKAN_LIBRARY_EXPORT window {
	window(window_properties);
	~window();

	window(window&& other) noexcept;
	window& operator=(window&& other) noexcept;
	window(window const& other);
	window& operator=(window const& other);

private:
	window_private *d;
};
} // namespace modern_vulkan