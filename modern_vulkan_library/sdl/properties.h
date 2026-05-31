#pragma once
#include <common/modern_vulkan_library.h>

namespace modern_vulkan::sdl
{
struct window_properties_private;
struct MODERN_VULKAN_LIBRARY_EXPORT window_properties {
	window_properties(std::string_view title, rect rect, bool vulkan_support);
	~window_properties();

	auto handle() const -> uint32_t;

	void title(std::string_view);
	void rect(rect);
	void vulkan_support(bool);

	void pos(position);
	void resizable(bool);

	window_properties(window_properties&& other) noexcept;
	window_properties& operator=(window_properties&& other) noexcept;
	window_properties(window_properties const& other) = delete;
	window_properties& operator=(window_properties const& other) = delete;

private:
	window_properties_private *d;
};
} // namespace modern_vulkan::sdl
