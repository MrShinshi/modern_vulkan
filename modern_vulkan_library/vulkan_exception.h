#pragma once
#include "modern_vulkan_library.h"

struct sdl_exception 
	: public std::exception {
	sdl_exception(
		std::string&& message, 
		std::stacktrace&& stack =  std::stacktrace::current());

	auto what() const noexcept 
		-> char const *	override;

	std::string const lib_error;
	std::string const sdl_error;
	std::stacktrace const stacktrace;

	mutable std::string what_str;
};
