#include <SDL3/SDL.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_vulkan.h>

#include <sdl/simple_direct_media_layer.h>
#include <common/modern_vulkan_exception.h>

#include <algorithm>
#include <stdexcept>
#include <cctype>
#include <stop_token>
#include <iostream>
#include <string_view>
#include <utility>

namespace modern_vulkan::sdl
{
namespace
{
void dispatch_input_event(input_callbacks const& callbacks, input_event const& event) {
	if (callbacks.any) {
		callbacks.any(event);
	}

	switch (event.device) {
	case input_device_type::mouse:
		if (callbacks.mouse) {
			callbacks.mouse(event);
		}
		break;

	case input_device_type::keyboard:
		if (callbacks.keyboard) {
			callbacks.keyboard(event);
		}
		break;

	case input_device_type::gamepad:
		if (callbacks.gamepad) {
			callbacks.gamepad(event);
		}
		break;
	}
}

auto parse_mouse_button(Uint8 button) -> mouse_button {
	switch (button) {
	case SDL_BUTTON_LEFT:
		return mouse_button::left;
	case SDL_BUTTON_MIDDLE:
		return mouse_button::middle;
	case SDL_BUTTON_RIGHT:
		return mouse_button::right;
	case SDL_BUTTON_X1:
		return mouse_button::x1;
	case SDL_BUTTON_X2:
		return mouse_button::x2;
	case 0:
		return mouse_button::none;
	default:
		return mouse_button::unknown;
	}
}

auto parse_gamepad_button(Uint8 button) -> gamepad_button {
	switch (button) {
	case SDL_GAMEPAD_BUTTON_SOUTH:
		return gamepad_button::south;
	case SDL_GAMEPAD_BUTTON_EAST:
		return gamepad_button::east;
	case SDL_GAMEPAD_BUTTON_WEST:
		return gamepad_button::west;
	case SDL_GAMEPAD_BUTTON_NORTH:
		return gamepad_button::north;
	case SDL_GAMEPAD_BUTTON_BACK:
		return gamepad_button::back;
	case SDL_GAMEPAD_BUTTON_GUIDE:
		return gamepad_button::guide;
	case SDL_GAMEPAD_BUTTON_START:
		return gamepad_button::start;
	case SDL_GAMEPAD_BUTTON_LEFT_STICK:
		return gamepad_button::left_stick;
	case SDL_GAMEPAD_BUTTON_RIGHT_STICK:
		return gamepad_button::right_stick;
	case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
		return gamepad_button::left_shoulder;
	case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
		return gamepad_button::right_shoulder;
	case SDL_GAMEPAD_BUTTON_DPAD_UP:
		return gamepad_button::dpad_up;
	case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
		return gamepad_button::dpad_down;
	case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
		return gamepad_button::dpad_left;
	case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
		return gamepad_button::dpad_right;
	case SDL_GAMEPAD_BUTTON_MISC1:
		return gamepad_button::misc1;
	case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1:
		return gamepad_button::right_paddle1;
	case SDL_GAMEPAD_BUTTON_LEFT_PADDLE1:
		return gamepad_button::left_paddle1;
	case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2:
		return gamepad_button::right_paddle2;
	case SDL_GAMEPAD_BUTTON_LEFT_PADDLE2:
		return gamepad_button::left_paddle2;
	case SDL_GAMEPAD_BUTTON_TOUCHPAD:
		return gamepad_button::touchpad;
	case SDL_GAMEPAD_BUTTON_MISC2:
		return gamepad_button::misc2;
	case SDL_GAMEPAD_BUTTON_MISC3:
		return gamepad_button::misc3;
	case SDL_GAMEPAD_BUTTON_MISC4:
		return gamepad_button::misc4;
	case SDL_GAMEPAD_BUTTON_MISC5:
		return gamepad_button::misc5;
	case SDL_GAMEPAD_BUTTON_MISC6:
		return gamepad_button::misc6;
	default:
		return gamepad_button::unknown;
	}
}

auto parse_gamepad_axis(Uint8 axis) -> gamepad_axis {
	switch (axis) {
	case SDL_GAMEPAD_AXIS_LEFTX:
		return gamepad_axis::left_x;
	case SDL_GAMEPAD_AXIS_LEFTY:
		return gamepad_axis::left_y;
	case SDL_GAMEPAD_AXIS_RIGHTX:
		return gamepad_axis::right_x;
	case SDL_GAMEPAD_AXIS_RIGHTY:
		return gamepad_axis::right_y;
	case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
		return gamepad_axis::left_trigger;
	case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:
		return gamepad_axis::right_trigger;
	default:
		return gamepad_axis::unknown;
	}
}

auto parse_instance_extension(char const *extension_name) -> instance_extension {
	auto const extension = std::string_view(extension_name);

	if (extension == "VK_KHR_surface") {
		return instance_extension::khr_surface;
	}

	if (extension == "VK_KHR_win32_surface") {
		return instance_extension::khr_win32_surface;
	}

	if (extension == "VK_KHR_xlib_surface") {
		return instance_extension::khr_xlib_surface;
	}

	if (extension == "VK_KHR_xcb_surface") {
		return instance_extension::khr_xcb_surface;
	}

	if (extension == "VK_KHR_wayland_surface") {
		return instance_extension::khr_wayland_surface;
	}

	if (extension == "VK_KHR_android_surface") {
		return instance_extension::khr_android_surface;
	}

	if (extension == "VK_EXT_metal_surface") {
		return instance_extension::ext_metal_surface;
	}

	if (extension == "VK_MVK_ios_surface") {
		return instance_extension::mvk_ios_surface;
	}

	if (extension == "VK_MVK_macos_surface") {
		return instance_extension::mvk_macos_surface;
	}

	throw std::runtime_error("Unsupported SDL Vulkan instance extension.");
}
} // namespace

struct simple_direct_media_layer_private {
	simple_direct_media_layer_private() {
		if (not SDL_Init(SDL_INIT_VIDEO)) {
			throw sdl::exception("SDL_Init failed");
		}
	}

	~simple_direct_media_layer_private() {
		SDL_Quit();
	}
	std::stop_source stop_source;
};

simple_direct_media_layer::simple_direct_media_layer()
	: d(new simple_direct_media_layer_private()) {
}

simple_direct_media_layer::~simple_direct_media_layer() {
	delete d;
}

simple_direct_media_layer::simple_direct_media_layer(simple_direct_media_layer&& other) noexcept
	: d(other.d) {
	other.d = nullptr;
}

simple_direct_media_layer& simple_direct_media_layer::operator=(simple_direct_media_layer&& other) noexcept {
	if (this != &other) {
		delete std::exchange(d, std::exchange(other.d, nullptr));
	}

	return *this;
}

auto simple_direct_media_layer::window(
	window_properties const& properties) -> sdl::window {
	return {properties};
}

auto simple_direct_media_layer::extensions() const -> std::vector<instance_extension> {

	Uint32 extension_count;

	auto extensions_ptr =
		SDL_Vulkan_GetInstanceExtensions(&extension_count);

	if (!extensions_ptr || extension_count == 0) {
		throw sdl::exception("SDL_Vulkan_GetInstanceExtensions failed");
	}

	std::vector<instance_extension> extensions;
	extensions.reserve(extension_count);

	for (Uint32 index{}; index < extension_count; ++index) {
		extensions.push_back(parse_instance_extension(extensions_ptr[index]));
	}

	return extensions;
}

auto simple_direct_media_layer::execute() -> int {
	return execute([] {});
}

auto simple_direct_media_layer::execute(std::function<void()> const& frame_action) -> int {
    return execute(frame_action, input_callbacks{});
}

auto simple_direct_media_layer::execute(std::function<void()> const& frame_action, std::function<void(input_event const&)> const& event_action) -> int {
	return execute(
		frame_action,
		input_callbacks{
			.any = event_action,
			.mouse = {},
			.keyboard = {},
			.gamepad = {}});
}

auto simple_direct_media_layer::execute(std::function<void()> const& frame_action, input_callbacks const& event_callbacks) -> int {

	while (!d->stop_source.stop_requested()) {
		SDL_Event event;

		while (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_EVENT_QUIT:
				d->stop_source.request_stop();
				break;

			default:
				// Do nothing.
				break;
			}

            switch (event.type) {
           case SDL_EVENT_MOUSE_BUTTON_DOWN:
				dispatch_input_event(event_callbacks, input_event{
					.type = input_event_type::pressed,
					.device = input_device_type::mouse,
					.button = parse_mouse_button(event.button.button),
                    .mouse_axis = mouse_axis::none,
					.key = '\0',
					.position = modern_vulkan::basic_position<float>{.x = event.button.x, .y = event.button.y},
					.wheel_delta_y = 0.0f,
					.gamepad_index = -1,
                    .gamepad_button = gamepad_button::none,
					.gamepad_axis = gamepad_axis::none,
					.gamepad_axis_value = 0.0f});
				break;

			case SDL_EVENT_MOUSE_BUTTON_UP:
               dispatch_input_event(event_callbacks, input_event{
					.type = input_event_type::released,
					.device = input_device_type::mouse,
					.button = parse_mouse_button(event.button.button),
                    .mouse_axis = mouse_axis::none,
					.key = '\0',
					.position = modern_vulkan::basic_position<float>{.x = event.button.x, .y = event.button.y},
					.wheel_delta_y = 0.0f,
					.gamepad_index = -1,
                    .gamepad_button = gamepad_button::none,
					.gamepad_axis = gamepad_axis::none,
					.gamepad_axis_value = 0.0f});
				break;

			case SDL_EVENT_MOUSE_MOTION:
               dispatch_input_event(event_callbacks, input_event{
					.type = input_event_type::motion,
					.device = input_device_type::mouse,
					.button = mouse_button::none,
                    .mouse_axis = mouse_axis::none,
					.key = '\0',
					.position = modern_vulkan::basic_position<float>{.x = event.motion.x, .y = event.motion.y},
					.wheel_delta_y = 0.0f,
					.gamepad_index = -1,
                    .gamepad_button = gamepad_button::none,
					.gamepad_axis = gamepad_axis::none,
					.gamepad_axis_value = 0.0f});
				break;

			case SDL_EVENT_MOUSE_WHEEL:
               dispatch_input_event(event_callbacks, input_event{
					.type = input_event_type::axis,
					.device = input_device_type::mouse,
					.button = mouse_button::none,
                    .mouse_axis = mouse_axis::wheel_vertical,
					.key = '\0',
					.position = modern_vulkan::basic_position<float>{.x = event.wheel.mouse_x, .y = event.wheel.mouse_y},
					.wheel_delta_y = event.wheel.y,
					.gamepad_index = -1,
                    .gamepad_button = gamepad_button::none,
					.gamepad_axis = gamepad_axis::none,
					.gamepad_axis_value = 0.0f});
				break;

			case SDL_EVENT_KEY_DOWN:
               dispatch_input_event(event_callbacks, input_event{
					.type = input_event_type::key_down,
					.device = input_device_type::keyboard,
					.button = mouse_button::none,
                 .mouse_axis = mouse_axis::none,
					.key = (event.key.key >= 0 && event.key.key <= 127)
						? static_cast<char>(std::tolower(static_cast<unsigned char>(event.key.key)))
						: '\0',
					.position = modern_vulkan::basic_position<float>{.x = 0.0f, .y = 0.0f},
					.wheel_delta_y = 0.0f,
					.gamepad_index = -1,
                    .gamepad_button = gamepad_button::none,
					.gamepad_axis = gamepad_axis::none,
					.gamepad_axis_value = 0.0f});
				break;

			case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
				dispatch_input_event(event_callbacks, input_event{
					.type = input_event_type::pressed,
					.device = input_device_type::gamepad,
					.button = mouse_button::none,
					.mouse_axis = mouse_axis::none,
					.key = '\0',
					.position = modern_vulkan::basic_position<float>{.x = 0.0f, .y = 0.0f},
					.wheel_delta_y = 0.0f,
					.gamepad_index = static_cast<int32_t>(event.gbutton.which),
					.gamepad_button = parse_gamepad_button(event.gbutton.button),
					.gamepad_axis = gamepad_axis::none,
					.gamepad_axis_value = 0.0f});
				break;

			case SDL_EVENT_GAMEPAD_BUTTON_UP:
				dispatch_input_event(event_callbacks, input_event{
					.type = input_event_type::released,
					.device = input_device_type::gamepad,
					.button = mouse_button::none,
					.mouse_axis = mouse_axis::none,
					.key = '\0',
					.position = modern_vulkan::basic_position<float>{.x = 0.0f, .y = 0.0f},
					.wheel_delta_y = 0.0f,
					.gamepad_index = static_cast<int32_t>(event.gbutton.which),
					.gamepad_button = parse_gamepad_button(event.gbutton.button),
					.gamepad_axis = gamepad_axis::none,
					.gamepad_axis_value = 0.0f});
				break;

			case SDL_EVENT_GAMEPAD_AXIS_MOTION:
				dispatch_input_event(event_callbacks, input_event{
					.type = input_event_type::axis,
					.device = input_device_type::gamepad,
					.button = mouse_button::none,
					.mouse_axis = mouse_axis::none,
					.key = '\0',
					.position = modern_vulkan::basic_position<float>{.x = 0.0f, .y = 0.0f},
					.wheel_delta_y = 0.0f,
					.gamepad_index = static_cast<int32_t>(event.gaxis.which),
					.gamepad_button = gamepad_button::none,
					.gamepad_axis = parse_gamepad_axis(event.gaxis.axis),
					.gamepad_axis_value = std::clamp(static_cast<float>(event.gaxis.value) / 32767.0f, -1.0f, 1.0f)});
				break;

			default:
				break;
			}
		}

		frame_action();

		SDL_Delay(10);
	}

	return EXIT_SUCCESS;
}
} // namespace modern_vulkan::sdl
