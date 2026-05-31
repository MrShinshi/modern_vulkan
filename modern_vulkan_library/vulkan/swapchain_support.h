#pragma once
#include <common/modern_vulkan_library.h>

#include <cstdint>
#include <vector>

namespace modern_vulkan
{
struct swapchain_surface_capabilities {
    uint32_t min_image_count = 0;
    uint32_t max_image_count = 0;
    rect current_extent{};
    bool current_extent_defined = false;
    rect min_image_extent{};
    rect max_image_extent{};
    uint32_t current_transform = 0;
};

struct swapchain_surface_format {
    int32_t format = 0;
    int32_t color_space = 0;
};

struct swapchain_support_details {
    swapchain_surface_capabilities capabilities{};
    std::vector<swapchain_surface_format> formats;
    std::vector<int32_t> present_modes;

    auto supported() const -> bool {
        return !formats.empty() && !present_modes.empty();
    }
};
} // namespace modern_vulkan
