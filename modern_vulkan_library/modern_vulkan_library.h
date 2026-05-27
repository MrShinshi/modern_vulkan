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
#include <exception>
#include <stacktrace>

namespace modern_vulkan
{
struct rect {
	std::size_t w;
	std::size_t h;
};
struct pos {
	std::size_t x;
	std::size_t y;
};

} // namespace modern_vulkan