#pragma once
#include <common/modern_vulkan_library.h>
#include <vulkan/point_cloud.h>

#include <array>
#include <filesystem>

namespace modern_vulkan
{
enum struct model_representation : uint8_t {
    point_cloud,
    mesh,
};

MODERN_VULKAN_LIBRARY_EXPORT auto load_model(
    std::filesystem::path const& path,
    model_representation representation,
    point const& color = {0.85f, 0.85f, 0.85f}) -> model_data;
} // namespace modern_vulkan
