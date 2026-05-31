#include <vulkan/logical_device.h>
#include <vulkan/physical_device.h>

#include <vulkan/vulkan.hpp>
#include <stdexcept>

#include <utility>

namespace modern_vulkan
{
namespace
{
auto device_extension_name(modern_vulkan::device_extension extension) -> char const * {
	switch (extension) {
	case modern_vulkan::device_extension::khr_swapchain:
		return VK_KHR_SWAPCHAIN_EXTENSION_NAME;
	}

	throw std::runtime_error("Unsupported Vulkan device extension.");
}

auto extension_names(std::vector<modern_vulkan::device_extension> const& extensions) -> std::vector<char const *> {
	std::vector<char const *> result;
	result.reserve(extensions.size());

	for (auto const extension : extensions) {
		result.push_back(device_extension_name(extension));
	}

	return result;
}

auto has_flag(queue_family_flags flags, queue_family_flags flag) -> bool {
	return static_cast<uint32_t>(flags & flag) != 0;
}

auto matches(queue_family_support const& support, queue_family_flags flags) -> bool {
	if (has_flag(flags, queue_family_flags::graphics) && !support.graphics) {
		return false;
	}

	if (has_flag(flags, queue_family_flags::compute) && !support.compute) {
		return false;
	}

	if (has_flag(flags, queue_family_flags::transfer) && !support.transfer) {
		return false;
	}

	if (has_flag(flags, queue_family_flags::sparse_binding) && !support.sparse_binding) {
		return false;
	}

	if (has_flag(flags, queue_family_flags::present) && !support.present) {
		return false;
	}

	return true;
}
} // namespace

struct logical_device_private {
	logical_device_private(
		modern_vulkan::physical_device const& bound_physical_device,
		modern_vulkan::surface const *surface,
		queue_family_flags flags,
		std::vector<device_extension> const& extensions)
		: physical_device_(bound_physical_device) {
		auto queue_families = surface != nullptr
								  ? physical_device_.queue_families(*surface, flags)
								  : physical_device_.queue_families(flags);
		if (!physical_device_.supports_extensions(extensions)) {
			throw std::runtime_error("Required device extensions are not supported.");
		}

		auto names = extension_names(extensions);
		if (queue_families.empty()) {
			throw std::runtime_error("At least one queue family is required to create a logical device.");
		}

		queue_families_.assign(queue_families.begin(), queue_families.end());

		std::vector<uint32_t> unique_queue_family_indices;
		unique_queue_family_indices.reserve(queue_families_.size());

		for (auto const& queue_family : queue_families_) {
			if (queue_family.queue_count == 0) {
				throw std::runtime_error("Queue family must expose at least one queue.");
			}

			if (std::ranges::find(unique_queue_family_indices, queue_family.index) == unique_queue_family_indices.end()) {
				unique_queue_family_indices.push_back(queue_family.index);
			}
		}

		std::vector<float> queue_priorities(unique_queue_family_indices.size(), 1.0f);
		std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
		queue_create_infos.reserve(unique_queue_family_indices.size());

		for (size_t queue_family_position{}; queue_family_position < unique_queue_family_indices.size(); ++queue_family_position) {
			queue_create_infos.push_back(VkDeviceQueueCreateInfo{
				.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				.queueFamilyIndex = unique_queue_family_indices[queue_family_position],
				.queueCount = 1,
				.pQueuePriorities = &queue_priorities[queue_family_position]});
		}

		VkPhysicalDeviceFeatures device_features{};
     if (physical_device_.supports_fill_mode_non_solid()) {
			device_features.fillModeNonSolid = VK_TRUE;
		}
		VkDeviceCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size()),
			.pQueueCreateInfos = queue_create_infos.data(),
			.enabledExtensionCount = static_cast<uint32_t>(names.size()),
			.ppEnabledExtensionNames = names.data(),
			.pEnabledFeatures = &device_features};

		if (vkCreateDevice(static_cast<VkPhysicalDevice>(physical_device_.handle()), &create_info, nullptr, &logical_device_) != VK_SUCCESS) {
			throw std::runtime_error("vkCreateDevice failed.");
		}
	}

	~logical_device_private() {
		if (logical_device_ != VK_NULL_HANDLE) {
			vkDeviceWaitIdle(logical_device_);
			vkDestroyDevice(logical_device_, nullptr);
		}
	}

	VkDevice logical_device_ = VK_NULL_HANDLE;
	modern_vulkan::physical_device physical_device_;
	std::vector<queue_family_support> queue_families_;
};

logical_device::logical_device(
	physical_device const& physical_device,
	surface const& surface,
	queue_family_flags flags,
	std::vector<device_extension> const& extensions)
	: d(new logical_device_private(
		  physical_device,
		  &surface,
		  flags,
		  extensions)) {
}

logical_device::~logical_device() {
	if (d == nullptr) {
		return;
	}

	delete d;
}

logical_device::logical_device(logical_device&& other) noexcept : d(other.d) {
	other.d = nullptr;
}

logical_device& logical_device::operator=(logical_device&& other) noexcept {
	if (this != &other) {
		delete std::exchange(d, std::exchange(other.d, nullptr));
	}

	return *this;
}

auto logical_device::handle() const -> logical_device_handle {
	if (d == nullptr) {
		return nullptr;
	}

	return static_cast<logical_device_handle>(d->logical_device_);
}

auto logical_device::bound_physical_device() const -> physical_device const& {
	if (d == nullptr) {
		throw std::runtime_error("Logical device is not initialized.");
	}

	return d->physical_device_;
}

auto logical_device::queue_families() const -> std::span<queue_family_support const> {
	if (d == nullptr) {
		return {};
	}

	return std::span<queue_family_support const>(d->queue_families_.data(), d->queue_families_.size());
}

auto logical_device::query_swapchain_support(surface const& surface) const -> swapchain_support_details {
	if (d == nullptr) {
		return {};
	}

	return swapchain_support_details{
		.capabilities = d->physical_device_.surface_capabilities(surface),
		.formats = d->physical_device_.surface_formats(surface),
		.present_modes = d->physical_device_.surface_present_modes(surface)};
}

void logical_device::wait_idle() const {
	if (d == nullptr || d->logical_device_ == VK_NULL_HANDLE) {
		return;
	}

	vkDeviceWaitIdle(d->logical_device_);
}
} // namespace modern_vulkan
