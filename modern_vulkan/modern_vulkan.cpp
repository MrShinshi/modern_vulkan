#include <iostream>
#include <vulkan/mesh_renderer.h>
#include <vulkan/stl_mesh.h>
#include <vulkan/point_cloud_renderer.h>
#include <vulkan/stl_point_cloud.h>
#include <sdl/simple_direct_media_layer.h>
#include <vulkan/extensions.h>
#include <vulkan/instance.h>
#include <vulkan/logical_device.h>
#include <vulkan/swapchain.h>

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <stdexcept>

int main() try {
	constexpr auto kLeftMouseButton = modern_vulkan::sdl::mouse_button::left;
	constexpr auto kRightMouseButton = modern_vulkan::sdl::mouse_button::right;
	enum struct render_mode {
		point_cloud,
		mesh,
	};

	modern_vulkan::sdl::simple_direct_media_layer sdl_layer;
	modern_vulkan::sdl::window_properties props(
		"Hello, Modern Vulkan!",
		{1280, 720},
		false);
	props.pos({100, 100});
	props.resizable(false);
	auto window = sdl_layer.window(props);

	std::vector<modern_vulkan::instance_layer> layers;
#if defined(_DEBUG)
	layers.push_back(modern_vulkan::instance_layer::khronos_validation);
#endif

	modern_vulkan::instance vk_instance(sdl_layer.extensions(), layers);
	auto const& surface = vk_instance.make_surface(window);

	auto physical_devices = vk_instance.physical_devices();
	if (physical_devices.empty()) {
		throw std::runtime_error("No Vulkan physical devices found.");
	}

	auto const& physical_device = physical_devices.front();
	modern_vulkan::logical_device logical_device(
		physical_device,
		surface,
		modern_vulkan::queue_family_flags::graphics | modern_vulkan::queue_family_flags::present,
		std::vector<modern_vulkan::device_extension>{modern_vulkan::device_extension::khr_swapchain});
	modern_vulkan::swapchain swapchain(logical_device, surface, {1280, 720});

	auto const stl_path = std::filesystem::path{LR"(G:\绯英_stls\obj_1_绯英.stl)"};
	auto const point_cloud = modern_vulkan::load_stl_point_cloud(stl_path, {0.95f, 0.78f, 0.86f});
	auto const mesh = modern_vulkan::load_stl_mesh(stl_path, {0.95f, 0.78f, 0.86f});
	std::cout << "Loaded " << point_cloud.vertices.size() << " unique points from STL." << std::endl;
	std::cout << "Loaded " << (mesh.vertices.size() / 3) << " triangles from STL." << std::endl;
	modern_vulkan::point_cloud_renderer point_renderer(logical_device, swapchain, point_cloud);
	modern_vulkan::mesh_renderer renderer(logical_device, swapchain, mesh);
	auto current_mode = render_mode::mesh;
	auto const title_prefix = std::string("Hello, Modern Vulkan!");
	auto last_fps_sample_time = std::chrono::steady_clock::now();
	uint32_t frames_since_last_sample = 0;

	return sdl_layer.execute(
		[&renderer, &point_renderer, &current_mode, &window, &title_prefix, &last_fps_sample_time, &frames_since_last_sample] {
			if (current_mode == render_mode::mesh) {
				renderer.draw_frame();
			} else {
				point_renderer.draw_frame();
			}
			++frames_since_last_sample;

			auto const now = std::chrono::steady_clock::now();
			auto const elapsed = now - last_fps_sample_time;
			if (elapsed >= std::chrono::milliseconds(250)) {
				auto const fps = static_cast<double>(frames_since_last_sample) /
								 std::chrono::duration<double>(elapsed).count();
				std::ostringstream title_stream;
				title_stream << title_prefix
							 << " | Mode: " << (current_mode == render_mode::mesh ? "Mesh" : "Points")
							 << " | Wireframe: " << (renderer.wireframe_enabled() ? "On" : "Off")
							 << " | Lighting: " << (renderer.lighting_enabled() ? "On" : "Off")
							 << " | FPS: " << std::fixed << std::setprecision(1) << fps;
				window.title(title_stream.str());
				frames_since_last_sample = 0;
				last_fps_sample_time = now;
			}
		},
		modern_vulkan::sdl::input_callbacks{
			.any = {},
			.mouse = [&renderer, &point_renderer](modern_vulkan::sdl::input_event const& event) {
				switch (event.type) {
				case modern_vulkan::sdl::input_event_type::pressed:
					if (event.button == kLeftMouseButton) {
						renderer.begin_rotate_drag(event.position.x, event.position.y);
						point_renderer.begin_rotate_drag(event.position.x, event.position.y);
					}
					else if (event.button == kRightMouseButton) {
						renderer.begin_pan_drag(event.position.x, event.position.y);
						point_renderer.begin_pan_drag(event.position.x, event.position.y);
					}
					break;

				case modern_vulkan::sdl::input_event_type::motion:
					renderer.update_rotate_drag(event.position.x, event.position.y);
					renderer.update_pan_drag(event.position.x, event.position.y);
					point_renderer.update_rotate_drag(event.position.x, event.position.y);
					point_renderer.update_pan_drag(event.position.x, event.position.y);
					break;

				case modern_vulkan::sdl::input_event_type::released:
					if (event.button == kLeftMouseButton) {
						renderer.end_rotate_drag();
						point_renderer.end_rotate_drag();
					}
					else if (event.button == kRightMouseButton) {
						renderer.end_pan_drag();
						point_renderer.end_pan_drag();
					}
					break;

				case modern_vulkan::sdl::input_event_type::axis:
					if (event.mouse_axis == modern_vulkan::sdl::mouse_axis::wheel_vertical) {
						renderer.zoom_camera(event.wheel_delta_y, event.position.x, event.position.y);
						point_renderer.zoom_camera(event.wheel_delta_y, event.position.x, event.position.y);
					}
					break;

				default:
					break;
				} },
			.keyboard = [&renderer, &current_mode](modern_vulkan::sdl::input_event const& event) {
				if (event.type != modern_vulkan::sdl::input_event_type::key_down) {
					return;
				}

				switch (event.key) {
				case '1':
					current_mode = render_mode::point_cloud;
					break;

				case '2':
					current_mode = render_mode::mesh;
					break;

				case 'w':
					renderer.toggle_wireframe();
					break;

				case 'l':
					renderer.toggle_lighting();
					break;

				default:
					break;
				} },
			.gamepad = [](modern_vulkan::sdl::input_event const&) {}});
}
catch (std::exception& e) {
	std::cerr << e.what() << std::endl;
}