#pragma once
#include <common/modern_vulkan_library.h>

#include <cstdint>

namespace modern_vulkan
{
struct physical_device;
struct logical_device;
struct surface;

struct swapchain_private;
struct MODERN_VULKAN_LIBRARY_EXPORT swapchain {
   swapchain(logical_device const& logical_device, surface const& surface, rect extent, void *old_swapchain = nullptr);
	~swapchain();

	swapchain(swapchain&& other) noexcept;
	swapchain& operator=(swapchain&& other) noexcept;

	swapchain(swapchain const&) = delete;
	swapchain& operator=(swapchain const&) = delete;

	auto extent() const -> rect;
	auto handle() const -> void *;
	auto image_format() const -> int32_t;
	auto image_count() const -> uint32_t;
	auto image_view_count() const -> uint32_t;
	auto image_view(uint32_t index) const -> void *;
	auto render_pass() const -> void *;
	auto framebuffer_count() const -> uint32_t;
	auto framebuffer(uint32_t index) const -> void *;

private:
	swapchain_private *d;
};
} // namespace modern_vulkan
