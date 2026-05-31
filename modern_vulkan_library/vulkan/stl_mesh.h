#pragma once
#include <common/modern_vulkan_library.h>
#include <vulkan/stl_point_cloud.h>

#include <array>
#include <filesystem>

namespace modern_vulkan
{
struct mesh_data {
    std::vector<point_vertex> vertices;
    point_cloud_bounds bounds;
};

MODERN_VULKAN_LIBRARY_EXPORT auto load_stl_mesh(
    std::filesystem::path const& path,
    std::array<float, 3> const& color = {0.85f, 0.85f, 0.85f}) -> mesh_data;
} // namespace modern_vulkan
