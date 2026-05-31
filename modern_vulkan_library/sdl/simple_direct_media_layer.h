#pragma once
#include <common/modern_vulkan_library.h>
#include <sdl/window.h>
#include <vulkan/extensions.h>

#include <functional>

namespace modern_vulkan::sdl
{
enum struct input_event_type : uint8_t {
	pressed,
	released,
	motion,
	axis,
	key_down,
};

enum struct input_device_type : uint8_t {
	mouse,
	keyboard,
	gamepad,
};

enum struct mouse_button : uint8_t {
	none = 0,
	left,
	middle,
	right,
	x1,
	x2,
	unknown = 0xff,
};

enum struct mouse_axis : uint8_t {
	none = 0,
	wheel_horizontal,
	wheel_vertical,
};

enum struct gamepad_button : uint8_t {
	none = 0,
	south,
	east,
	west,
	north,
	back,
	guide,
	start,
	left_stick,
	right_stick,
	left_shoulder,
	right_shoulder,
	dpad_up,
	dpad_down,
	dpad_left,
	dpad_right,
	misc1,
	right_paddle1,
	left_paddle1,
	right_paddle2,
	left_paddle2,
	touchpad,
	misc2,
	misc3,
	misc4,
	misc5,
	misc6,
	unknown = 0xff,
};

enum struct gamepad_axis : uint8_t {
	none = 0,
	left_x,
	left_y,
	right_x,
	right_y,
	left_trigger,
	right_trigger,
	unknown = 0xff,
};

struct input_event {
	input_event_type type;
	input_device_type device;
	mouse_button button;
   mouse_axis mouse_axis;
	char key;
    modern_vulkan::basic_position<float> position;
	float wheel_delta_y;
	int32_t gamepad_index;
 gamepad_button gamepad_button;
	gamepad_axis gamepad_axis;
	float gamepad_axis_value;
};

struct input_callbacks {
	std::function<void(input_event const&)> any;
	std::function<void(input_event const&)> mouse;
	std::function<void(input_event const&)> keyboard;
	std::function<void(input_event const&)> gamepad;
};

struct simple_direct_media_layer_private;
struct MODERN_VULKAN_LIBRARY_EXPORT simple_direct_media_layer {
	simple_direct_media_layer();
	~simple_direct_media_layer();

	simple_direct_media_layer(simple_direct_media_layer&& other) noexcept;
	simple_direct_media_layer& operator=(simple_direct_media_layer&& other) noexcept;

	simple_direct_media_layer(simple_direct_media_layer const&) = delete;
	simple_direct_media_layer& operator=(simple_direct_media_layer const&) = delete;

	auto window(window_properties const&) -> window;
	auto extensions() const -> std::vector<instance_extension>;

	auto execute() -> int;
	auto execute(std::function<void()> const& frame_action) -> int;
	auto execute(std::function<void()> const& frame_action, std::function<void(input_event const&)> const& event_action) -> int;
	auto execute(std::function<void()> const& frame_action, input_callbacks const& event_callbacks) -> int;

private:
	simple_direct_media_layer_private *d;
};
} // namespace modern_vulkan::sdl