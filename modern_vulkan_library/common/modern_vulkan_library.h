#pragma once

#ifndef BUILD_STATIC
#if defined(MODERN_VULKAN_LIBRARY_EXPORTS)
#define MODERN_VULKAN_LIBRARY_EXPORT __declspec(dllexport)
#else
#define MODERN_VULKAN_LIBRARY_EXPORT __declspec(dllimport)
#endif
#else
#define MODERN_VULKAN_LIBRARY_EXPORT
#endif

#include <memory>
#include <string>
#include <vector>
#include <span>
#include <exception>
#include <stacktrace>

namespace modern_vulkan
{
template <typename value_type>
struct basic_rect {
	value_type w;
	value_type h;
};

using rect = basic_rect<std::size_t>;

template <typename value_type>
struct basic_position {
	value_type x;
	value_type y;
};

using position = basic_position<std::size_t>;

} // namespace modern_vulkan