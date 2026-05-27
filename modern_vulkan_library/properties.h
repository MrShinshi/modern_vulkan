#pragma once
#include "modern_vulkan_library.h"

namespace modern_vulkan
{
struct window_properties_private;
struct MODERN_VULKAN_LIBRARY_EXPORT window_properties {
	window_properties(std::string_view title, rect rect, bool vulkan_support);
	~window_properties();

	void title(std::string_view);
	void rect(rect);
	void vulkan_support(bool);

	void pos(pos);

	window_properties(window_properties&& other) noexcept;
	window_properties& operator=(window_properties&& other) noexcept;
	window_properties(window_properties const& other);
	window_properties& operator=(window_properties const& other);

private:
	friend struct window;
	window_properties_private *d;
};

} // namespace modern_vulkan
