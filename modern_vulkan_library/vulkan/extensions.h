#pragma once

namespace modern_vulkan
{
enum struct instance_extension {
	khr_surface,
	khr_win32_surface,
	khr_xlib_surface,
	khr_xcb_surface,
	khr_wayland_surface,
	khr_android_surface,
	ext_metal_surface,
	mvk_ios_surface,
	mvk_macos_surface,
};

enum struct instance_layer {
	khronos_validation,
};

enum struct device_extension {
	khr_swapchain,
};
} // namespace modern_vulkan
