#pragma once
#include <common/modern_vulkan_library.h>

#include <cstdint>

namespace modern_vulkan
{
enum struct queue_family_flags : uint32_t {
	none = 0,
	graphics = 1 << 0,
	compute = 1 << 1,
	transfer = 1 << 2,
	sparse_binding = 1 << 3,
	present = 1 << 4,
};

auto MODERN_VULKAN_LIBRARY_EXPORT operator|(queue_family_flags left, queue_family_flags right) -> queue_family_flags;
auto MODERN_VULKAN_LIBRARY_EXPORT operator&(queue_family_flags left, queue_family_flags right) -> queue_family_flags;

struct MODERN_VULKAN_LIBRARY_EXPORT queue_family_support {
	uint32_t index;
	uint32_t queue_count;
	bool graphics;
	bool compute;
	bool transfer;
	bool sparse_binding;
	bool present;
};
} // namespace modern_vulkan
