#pragma once
#include <common/modern_vulkan_library.h>

#include <array>
#include <filesystem>

namespace modern_vulkan
{
struct logical_device;
struct swapchain;
struct point_cloud_pipeline_private;

enum struct primitive_topology {
	point_list,
	triangle_list,
};

enum struct polygon_mode {
	solid,
	line,
};

struct MODERN_VULKAN_LIBRARY_EXPORT point_cloud_pipeline {
	point_cloud_pipeline(
		logical_device const& logical_device,
		swapchain const& swapchain,
		std::filesystem::path const& vertex_shader_path,
		std::filesystem::path const& fragment_shader_path,
		primitive_topology topology = primitive_topology::point_list,
		polygon_mode polygon = polygon_mode::solid);
	~point_cloud_pipeline();

	point_cloud_pipeline(point_cloud_pipeline&& other) noexcept;
	point_cloud_pipeline& operator=(point_cloud_pipeline&& other) noexcept;

	point_cloud_pipeline(point_cloud_pipeline const&) = delete;
	point_cloud_pipeline& operator=(point_cloud_pipeline const&) = delete;

	auto handle() const -> void *;
	auto layout() const -> void *;

private:
	point_cloud_pipeline_private *d;
};
} // namespace modern_vulkan
