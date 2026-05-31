#pragma once
#include <common/modern_vulkan_library.h>
#include <vulkan/device_queue_family.h>
#include <vulkan/extensions.h>
#include <vulkan/swapchain_support.h>

#include <span>
#include <cstdint>
#include <string_view>
#include <vector>

namespace modern_vulkan
{
struct instance;
struct surface;
struct physical_device_private;
struct MODERN_VULKAN_LIBRARY_EXPORT physical_device {
	using physical_device_handle = void *;
	physical_device(instance const&, physical_device_handle);
	~physical_device();

	physical_device(physical_device&& other) noexcept;
	physical_device& operator=(physical_device&& other) noexcept;
	physical_device(physical_device const& other);
	physical_device& operator=(physical_device const& other);

	auto handle() const -> physical_device_handle;
	auto name() const -> std::string_view;
	auto is_discrete() const -> bool;
    auto supports_fill_mode_non_solid() const -> bool;
	auto queue_families(queue_family_flags flags = queue_family_flags::none) const -> std::vector<queue_family_support>;
	auto queue_families(surface const& surface, queue_family_flags flags = queue_family_flags::none) const -> std::vector<queue_family_support>;
	auto supports_extensions(std::vector<device_extension> const& extensions) const -> bool;
	auto supports_presentation(surface const& surface, uint32_t queue_family_index) const -> bool;
	auto surface_format_count(surface const& surface) const -> uint32_t;
	auto surface_present_mode_count(surface const& surface) const -> uint32_t;
	auto swapchain_supported(surface const& surface) const -> bool;

	auto surface_capabilities(surface const& surface) const -> swapchain_surface_capabilities;
	auto surface_formats(surface const& surface) const -> std::vector<swapchain_surface_format>;
	auto surface_present_modes(surface const& surface) const -> std::vector<int32_t>;

private:
	physical_device_private *d;
};
} // namespace modern_vulkan
