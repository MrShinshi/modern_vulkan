#pragma once
#include "modern_vulkan_library.h"

#include "window.h"

namespace modern_vulkan
{
struct simple_direct_media_layer_private;
struct MODERN_VULKAN_LIBRARY_EXPORT simple_direct_media_layer {
	simple_direct_media_layer();
	~simple_direct_media_layer();

	auto window(window_properties const&) -> window;
	auto extensions() const -> std::vector<std::string>;

	auto execute() -> int;

private:
	simple_direct_media_layer_private* d;
};
} // namespace modern_vulkan