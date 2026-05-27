#include <iostream>
#include <simple_direct_media_layer.h>

int main() try {
	modern_vulkan::simple_direct_media_layer sdl_layer;
	modern_vulkan::window_properties props(
		"Hello, Modern Vulkan!",
		{1280, 720},
		false);
	props.pos({100, 100});
	auto window = sdl_layer.window(props);

	return sdl_layer.execute();
}
catch (std::exception& e) {
	std::cerr << e.what() << std::endl;
}