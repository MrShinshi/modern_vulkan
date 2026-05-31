#pragma once
#include "modern_vulkan_library.h"

namespace modern_vulkan::sdl
{
struct exception
	: public std::exception {
	exception(
		std::string&& message,
		std::stacktrace&& stack = std::stacktrace::current());

	auto what() const noexcept
		-> char const * override;

	std::string const lib_error;
	std::string const sdl_error;
	std::stacktrace const stacktrace;

	mutable std::string what_str;
};

}
