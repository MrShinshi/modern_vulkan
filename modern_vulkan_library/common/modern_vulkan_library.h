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
#include <array>
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

template <typename value_type>
struct basic_point {
	value_type x{};
	value_type y{};
	value_type z{};

	constexpr basic_point() = default;
	constexpr basic_point(value_type x_value, value_type y_value, value_type z_value) noexcept
		: x(x_value), y(y_value), z(z_value) {
	}
	constexpr basic_point(std::array<value_type, 3> const& values) noexcept
		: x(values[0]), y(values[1]), z(values[2]) {
	}

	auto operator=(std::array<value_type, 3> const& values) noexcept -> basic_point& {
		x = values[0];
		y = values[1];
		z = values[2];
		return *this;
	}

	constexpr operator std::array<value_type, 3>() const noexcept {
		return {x, y, z};
	}

    static constexpr auto size() noexcept -> std::size_t {
		return 3;
	}
   constexpr auto operator[](std::size_t index) noexcept -> value_type& {
		switch (index) {
		case 0:
			return x;
		case 1:
			return y;
		default:
			return z;
		}
	}
   constexpr auto operator[](std::size_t index) const noexcept -> value_type const& {
		switch (index) {
		case 0:
			return x;
		case 1:
			return y;
		default:
			return z;
		}
	}
};

using position = basic_position<std::size_t>;
using point = basic_point<float>;

} // namespace modern_vulkan