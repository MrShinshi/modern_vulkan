#include <vulkan/device_queue_family.h>

namespace modern_vulkan
{
auto operator|(queue_family_flags left, queue_family_flags right) -> queue_family_flags {
	return static_cast<queue_family_flags>(static_cast<uint32_t>(left) | static_cast<uint32_t>(right));
}

auto operator&(queue_family_flags left, queue_family_flags right) -> queue_family_flags {
	return static_cast<queue_family_flags>(static_cast<uint32_t>(left) & static_cast<uint32_t>(right));
}
} // namespace modern_vulkan
