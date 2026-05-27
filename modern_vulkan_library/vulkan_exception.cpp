#include "vulkan_exception.h"
#include <SDL3/SDL.h>
#include <sstream>

sdl_exception::sdl_exception(
	std::string&& message,
	std::stacktrace&& stack)
	: lib_error(std::move(message)),
	  sdl_error(SDL_GetError()),
	  stacktrace(std::move(stack)) {}
	
char const *sdl_exception::what() const noexcept {
	try {
		constexpr auto reset = "\x1b[0m";
		constexpr auto bold = "\x1b[1m";
		constexpr auto red = "\x1b[31m";
		constexpr auto yellow = "\x1b[33m";
		constexpr auto cyan = "\x1b[36m";
		constexpr auto gray = "\x1b[90m";
		constexpr auto orange = "\x1b[38;2;214;157;133m";

		std::ostringstream oss;

		oss << bold << red
			<< "● SDL EXCEPTION\n"
			<< reset
			<< "\n"
			<< bold << yellow << "● Title ：" << reset << lib_error << "\n\n"
			<< bold << orange << "● SDL Error ：" << reset << sdl_error << "\n"
			<< "\n"
			<< bold << cyan << "● Stacktrace ：\n"
			<< reset << "\n"
			<< gray << stacktrace << reset
			<< "\n";

		what_str = oss.str();
		return what_str.c_str();
	}
	catch (...) {
		return "sdl_exception::what() failed";
	}
}