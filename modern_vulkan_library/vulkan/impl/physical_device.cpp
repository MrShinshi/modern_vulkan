#include <vulkan/physical_device.h>
#include <vulkan/instance.h>
#include <vulkan/surface.h>

#include <vulkan/vulkan.hpp>

#include <algorithm>
#include <limits>
#include <string_view>
#include <utility>

namespace modern_vulkan
{
namespace
{
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

auto supports_extension_names(VkPhysicalDevice physical_device, std::span<char const *> extensions) -> bool {
	uint32_t extension_count = 0;
	vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, nullptr);

	std::vector<VkExtensionProperties> available_extensions(extension_count);
	vkEnumerateDeviceExtensionProperties(
		physical_device,
		nullptr,
		&extension_count,
		available_extensions.data());

	return std::ranges::all_of(extensions, [&](char const *extension) {
		return std::ranges::any_of(available_extensions, [&](VkExtensionProperties const& available_extension) {
			return std::string_view(available_extension.extensionName) == extension;
		});
	});
}
} // namespace

struct physical_device_private {
	instance const *instance_;
	VkPhysicalDevice physical_device_;
	VkPhysicalDeviceFeatures device_features_;
	VkPhysicalDeviceProperties device_properties_;
};

physical_device::physical_device(instance const& instance, physical_device_handle physical_device) : d(new physical_device_private) {
	if (physical_device == nullptr) {
		throw std::runtime_error("Physical device handle cannot be null.");
	}

	auto const vk_physical_device = static_cast<VkPhysicalDevice>(physical_device);
	uint32_t device_count = 0;
	vkEnumeratePhysicalDevices(static_cast<VkInstance>(instance.handle()), &device_count, nullptr);
	std::vector<VkPhysicalDevice> devices(device_count);
	if (device_count > 0) {
		vkEnumeratePhysicalDevices(static_cast<VkInstance>(instance.handle()), &device_count, devices.data());
	}

	if (std::ranges::find(devices, vk_physical_device) == devices.end()) {
		throw std::runtime_error("Physical device handle is not valid for the Vulkan instance.");
	}

	d->instance_ = &instance;
	d->physical_device_ = vk_physical_device;

	vkGetPhysicalDeviceFeatures(d->physical_device_, &d->device_features_);
	vkGetPhysicalDeviceProperties(d->physical_device_, &d->device_properties_);
}

physical_device::~physical_device() {
	delete d;
}

physical_device::physical_device(physical_device&& other) noexcept : d(other.d) {
	other.d = nullptr;
}

physical_device& physical_device::operator=(physical_device&& other) noexcept {
	if (this != &other) {
		delete std::exchange(d, std::exchange(other.d, nullptr));
	}
	return *this;
}

physical_device::physical_device(physical_device const& other)
	: d(other.d ? new physical_device_private(*other.d) : nullptr) {
}

physical_device& physical_device::operator=(physical_device const& other) {
	if (this != &other) {
#pragma warning(push)
#pragma warning(disable : 6011)
		delete std::exchange(d, other.d ? new physical_device_private(*other.d) : nullptr);
#pragma warning(pop)
	}
	return *this;
}

auto physical_device::name() const -> std::string_view {
	return d->device_properties_.deviceName;
}

auto physical_device::is_discrete() const -> bool {
	return d->device_properties_.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
}

auto physical_device::supports_fill_mode_non_solid() const -> bool {
	return d->device_features_.fillModeNonSolid == VK_TRUE;
}

auto physical_device::handle() const -> physical_device_handle {
	return static_cast<physical_device_handle>(d->physical_device_);
}

auto physical_device::queue_families(queue_family_flags flags) const -> std::vector<queue_family_support> {
	uint32_t queue_family_count{};
	vkGetPhysicalDeviceQueueFamilyProperties(d->physical_device_, &queue_family_count, nullptr);

	std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(d->physical_device_, &queue_family_count, queue_families.data());

	std::vector<queue_family_support> result;
	result.reserve(queue_families.size());

	for (uint32_t queue_family_index{}; queue_family_index < queue_family_count; ++queue_family_index) {
		auto const& queue_family = queue_families[queue_family_index];
		auto support = queue_family_support{.index = queue_family_index,
											.queue_count = queue_family.queueCount,
											.graphics = static_cast<bool>(queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT),
											.compute = static_cast<bool>(queue_family.queueFlags & VK_QUEUE_COMPUTE_BIT),
											.transfer = static_cast<bool>(queue_family.queueFlags & VK_QUEUE_TRANSFER_BIT),
											.sparse_binding = static_cast<bool>(queue_family.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT),
											.present = false};

		if (matches(support, flags)) {
			result.push_back(support);
		}
	}

	return result;
}

auto physical_device::queue_families(surface const& surface, queue_family_flags flags) const -> std::vector<queue_family_support> {
	uint32_t queue_family_count{};
	vkGetPhysicalDeviceQueueFamilyProperties(d->physical_device_, &queue_family_count, nullptr);

	std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(d->physical_device_, &queue_family_count, queue_families.data());

	std::vector<queue_family_support> result;
	result.reserve(queue_families.size());

	for (uint32_t queue_family_index{}; queue_family_index < queue_family_count; ++queue_family_index) {
		auto const& queue_family = queue_families[queue_family_index];
		auto support = queue_family_support{.index = queue_family_index,
											.queue_count = queue_family.queueCount,
											.graphics = static_cast<bool>(queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT),
											.compute = static_cast<bool>(queue_family.queueFlags & VK_QUEUE_COMPUTE_BIT),
											.transfer = static_cast<bool>(queue_family.queueFlags & VK_QUEUE_TRANSFER_BIT),
											.sparse_binding = static_cast<bool>(queue_family.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT),
											.present = supports_presentation(surface, queue_family_index)};

		if (matches(support, flags)) {
			result.push_back(support);
		}
	}

	return result;
}

auto physical_device::supports_extensions(std::vector<device_extension> const& extensions) const -> bool {
	auto names = extension_names(extensions);
	return supports_extension_names(d->physical_device_, std::span<char const *>(names.data(), names.size()));
}

auto physical_device::supports_presentation(surface const& surface, uint32_t queue_family_index) const -> bool {
	VkBool32 present_support = VK_FALSE;
	vkGetPhysicalDeviceSurfaceSupportKHR(
		d->physical_device_,
		queue_family_index,
		static_cast<VkSurfaceKHR>(surface.handle()),
		&present_support);
	return present_support == VK_TRUE;
}

auto physical_device::surface_capabilities(surface const& surface) const -> swapchain_surface_capabilities {
	VkSurfaceCapabilitiesKHR capabilities{};
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
		d->physical_device_,
		static_cast<VkSurfaceKHR>(surface.handle()),
		&capabilities);
	auto result = swapchain_surface_capabilities{};
	result.min_image_count = capabilities.minImageCount;
	result.max_image_count = capabilities.maxImageCount;
	result.current_extent = rect{.w = capabilities.currentExtent.width, .h = capabilities.currentExtent.height};
	result.current_extent_defined = capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max();
	result.min_image_extent = rect{.w = capabilities.minImageExtent.width, .h = capabilities.minImageExtent.height};
	result.max_image_extent = rect{.w = capabilities.maxImageExtent.width, .h = capabilities.maxImageExtent.height};
	result.current_transform = static_cast<uint32_t>(capabilities.currentTransform);
	return result;
}

auto physical_device::surface_formats(surface const& surface) const -> std::vector<swapchain_surface_format> {
	uint32_t format_count = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(
		d->physical_device_,
		static_cast<VkSurfaceKHR>(surface.handle()),
		&format_count,
		nullptr);

	std::vector<VkSurfaceFormatKHR> formats(format_count);
	if (format_count > 0) {
		vkGetPhysicalDeviceSurfaceFormatsKHR(
			d->physical_device_,
			static_cast<VkSurfaceKHR>(surface.handle()),
			&format_count,
			formats.data());
	}

	std::vector<swapchain_surface_format> result;
	result.reserve(formats.size());
	for (auto const& format : formats) {
		result.push_back(swapchain_surface_format{
			.format = static_cast<int32_t>(format.format),
			.color_space = static_cast<int32_t>(format.colorSpace)});
	}

	return result;
}

auto physical_device::surface_present_modes(surface const& surface) const -> std::vector<int32_t> {
	uint32_t present_mode_count = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(
		d->physical_device_,
		static_cast<VkSurfaceKHR>(surface.handle()),
		&present_mode_count,
		nullptr);

	std::vector<VkPresentModeKHR> present_modes(present_mode_count);
	if (present_mode_count > 0) {
		vkGetPhysicalDeviceSurfacePresentModesKHR(
			d->physical_device_,
			static_cast<VkSurfaceKHR>(surface.handle()),
			&present_mode_count,
			present_modes.data());
	}

	std::vector<int32_t> result;
	result.reserve(present_modes.size());
	for (auto const present_mode : present_modes) {
		result.push_back(static_cast<int32_t>(present_mode));
	}

	return result;
}

auto physical_device::surface_format_count(surface const& surface) const -> uint32_t {
	uint32_t format_count = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(
		d->physical_device_,
		static_cast<VkSurfaceKHR>(surface.handle()),
		&format_count,
		nullptr);
	return format_count;
}

auto physical_device::surface_present_mode_count(surface const& surface) const -> uint32_t {
	uint32_t present_mode_count = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(
		d->physical_device_,
		static_cast<VkSurfaceKHR>(surface.handle()),
		&present_mode_count,
		nullptr);
	return present_mode_count;
}

auto physical_device::swapchain_supported(surface const& surface) const -> bool {
	return surface_format_count(surface) > 0 && surface_present_mode_count(surface) > 0;
}
} // namespace modern_vulkan