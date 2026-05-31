#pragma once
#include <common/modern_vulkan_library.h>

#include <vector>

namespace modern_vulkan
{
struct point_vertex {
    point position{0.0f, 0.0f, 0.0f};
    point color{1.0f, 1.0f, 1.0f};
    point normal{0.0f, 0.0f, 1.0f};
};

struct bounds {
    point min{0.0f, 0.0f, 0.0f};
    point max{0.0f, 0.0f, 0.0f};
};

struct model_data {
    std::vector<point_vertex> vertices;
    bounds bounds{};
};
} // namespace modern_vulkan
