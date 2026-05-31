#pragma once
#include <common/modern_vulkan_library.h>
#include <vulkan/point_cloud_pipeline.h>

#include <array>
#include <filesystem>

namespace modern_vulkan
{
struct point_cloud_bounds {
    std::array<float, 3> min;
    std::array<float, 3> max;
};

struct point_cloud_data {
    std::vector<point_vertex> vertices;
    point_cloud_bounds bounds;
};

MODERN_VULKAN_LIBRARY_EXPORT auto load_stl_point_cloud(
    std::filesystem::path const& path,
    std::array<float, 3> const& color = {0.85f, 0.85f, 0.85f}) -> point_cloud_data;
} // namespace modern_vulkan
