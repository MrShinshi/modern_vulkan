#pragma once
#include <common/modern_vulkan_library.h>
#include <vulkan/point_cloud.h>

namespace modern_vulkan
{
struct logical_device;
struct swapchain;
struct point_cloud_renderer_private;

struct MODERN_VULKAN_LIBRARY_EXPORT point_cloud_renderer {
	point_cloud_renderer(
		logical_device const& logical_device,
		swapchain const& swapchain,
		model_data const& point_cloud);
	~point_cloud_renderer();

	point_cloud_renderer(point_cloud_renderer&& other) noexcept;
	point_cloud_renderer& operator=(point_cloud_renderer&& other) noexcept;

	point_cloud_renderer(point_cloud_renderer const&) = delete;
	point_cloud_renderer& operator=(point_cloud_renderer const&) = delete;

	void begin_rotate_drag(float x, float y);
	void begin_pan_drag(float x, float y);
	void update_rotate_drag(float x, float y);
	void end_rotate_drag();
	void update_pan_drag(float x, float y);
	void end_pan_drag();
	void zoom_camera(float delta, float x, float y);
	void draw_frame() const;

private:
	point_cloud_renderer_private *d;
};
} // namespace modern_vulkan
