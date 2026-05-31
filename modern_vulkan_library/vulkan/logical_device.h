#pragma once
#include <common/modern_vulkan_library.h>
#include <vulkan/device_queue_family.h>
#include <vulkan/extensions.h>
#include <vulkan/swapchain_support.h>

#include <span>
#include <vector>

namespace modern_vulkan
{
struct physical_device;
struct surface;
struct logical_device_private;
struct MODERN_VULKAN_LIBRARY_EXPORT logical_device {
	logical_device(
		physical_device const& physical_device,
		surface const& surface,
		queue_family_flags flags,
		std::vector<device_extension> const& extensions = {});
	~logical_device();

	logical_device(logical_device&& other) noexcept;
	logical_device& operator=(logical_device&& other) noexcept;

	logical_device(logical_device const&) = delete;
	logical_device& operator=(logical_device const&) = delete;

	using logical_device_handle = void *;
	auto handle() const -> logical_device_handle;
	auto bound_physical_device() const -> physical_device const&;

	auto queue_families() const -> std::span<queue_family_support const>;
	auto query_swapchain_support(surface const& surface) const -> swapchain_support_details;

	void wait_idle() const;

private:
	logical_device_private *d;
};
} // namespace modern_vulkan
