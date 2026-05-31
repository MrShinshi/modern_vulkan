#include <vulkan/instance.h>
#include <vulkan/surface.h>
#include <sdl/window.h>
#include <sdl/impl/window_private.h>

#include <vulkan/vulkan.hpp>
#include <vector>
#include <optional>
#include <utility>

namespace
{
auto instance_extension_name(modern_vulkan::instance_extension extension) -> char const * {
	switch (extension) {
	case modern_vulkan::instance_extension::khr_surface:
#ifdef VK_KHR_SURFACE_EXTENSION_NAME
		return VK_KHR_SURFACE_EXTENSION_NAME;
#else
		return "VK_KHR_surface";
#endif
	case modern_vulkan::instance_extension::khr_win32_surface:
#ifdef VK_KHR_WIN32_SURFACE_EXTENSION_NAME
		return VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
#else
		return "VK_KHR_win32_surface";
#endif
	case modern_vulkan::instance_extension::khr_xlib_surface:
#ifdef VK_KHR_XLIB_SURFACE_EXTENSION_NAME
		return VK_KHR_XLIB_SURFACE_EXTENSION_NAME;
#else
		return "VK_KHR_xlib_surface";
#endif
	case modern_vulkan::instance_extension::khr_xcb_surface:
#ifdef VK_KHR_XCB_SURFACE_EXTENSION_NAME
		return VK_KHR_XCB_SURFACE_EXTENSION_NAME;
#else
		return "VK_KHR_xcb_surface";
#endif
	case modern_vulkan::instance_extension::khr_wayland_surface:
#ifdef VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME
		return VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME;
#else
		return "VK_KHR_wayland_surface";
#endif
	case modern_vulkan::instance_extension::khr_android_surface:
#ifdef VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
		return VK_KHR_ANDROID_SURFACE_EXTENSION_NAME;
#else
		return "VK_KHR_android_surface";
#endif
	case modern_vulkan::instance_extension::ext_metal_surface:
#ifdef VK_EXT_METAL_SURFACE_EXTENSION_NAME
		return VK_EXT_METAL_SURFACE_EXTENSION_NAME;
#else
		return "VK_EXT_metal_surface";
#endif
	case modern_vulkan::instance_extension::mvk_ios_surface:
#ifdef VK_MVK_IOS_SURFACE_EXTENSION_NAME
		return VK_MVK_IOS_SURFACE_EXTENSION_NAME;
#else
		return "VK_MVK_ios_surface";
#endif
	case modern_vulkan::instance_extension::mvk_macos_surface:
#ifdef VK_MVK_MACOS_SURFACE_EXTENSION_NAME
		return VK_MVK_MACOS_SURFACE_EXTENSION_NAME;
#else
		return "VK_MVK_macos_surface";
#endif
	}

	throw std::runtime_error("Unsupported Vulkan instance extension.");
}

auto instance_layer_name(modern_vulkan::instance_layer layer) -> char const * {
	switch (layer) {
	case modern_vulkan::instance_layer::khronos_validation:
		return "VK_LAYER_KHRONOS_validation";
	}

	throw std::runtime_error("Unsupported Vulkan instance layer.");
}

auto supported_layers(std::span<char const *> requested_layers) -> std::vector<char const *> {
	auto const available_layers = vk::enumerateInstanceLayerProperties();
	std::vector<char const *> enabled_layers;

	for (auto const requested_layer : requested_layers) {
		for (auto const& available_layer : available_layers) {
			if (std::string_view(available_layer.layerName) == requested_layer) {
				enabled_layers.push_back(requested_layer);
				break;
			}
		}
	}

	return enabled_layers;
}

auto extension_names(std::span<modern_vulkan::instance_extension const> extensions) -> std::vector<char const *> {
	std::vector<char const *> result;
	result.reserve(extensions.size());

	for (auto const extension : extensions) {
		result.push_back(instance_extension_name(extension));
	}

	return result;
}

auto layer_names(std::span<modern_vulkan::instance_layer const> layers) -> std::vector<char const *> {
	std::vector<char const *> result;
	result.reserve(layers.size());

	for (auto const layer : layers) {
		result.push_back(instance_layer_name(layer));
	}

	return result;
}

auto create_instance_handle(std::span<char const *> extensions, std::span<char const *> layers) -> vk::Instance {
	auto enabled_layers = supported_layers(layers);
	auto app_info = vk::ApplicationInfo()
						.setApiVersion(VK_API_VERSION_1_4);
	auto instance_info = vk::InstanceCreateInfo()
							 .setFlags(vk::InstanceCreateFlags())
							 .setPApplicationInfo(&app_info)
							 .setEnabledExtensionCount(static_cast<uint32_t>(extensions.size()))
							 .setPpEnabledExtensionNames(extensions.data())
							 .setEnabledLayerCount(static_cast<uint32_t>(enabled_layers.size()))
							 .setPpEnabledLayerNames(enabled_layers.data());

	return vk::createInstance(instance_info);
}
} // namespace

namespace modern_vulkan
{
struct instance_private {
	vk::Instance instance_;
	std::optional<surface> surface_;
};
instance::instance(
	std::span<instance_extension const> extensions,
	std::span<instance_layer const> layers)
	: d(new instance_private()) {
	auto extension_name_list = extension_names(extensions);
	auto layer_name_list = layer_names(layers);
	d->instance_ = create_instance_handle(
		std::span<char const *>(extension_name_list.data(), extension_name_list.size()),
		std::span<char const *>(layer_name_list.data(), layer_name_list.size()));
}

instance::~instance() {
	delete d;
}

instance::instance(instance&& other) noexcept : d(other.d) {
	other.d = nullptr;
}

instance& instance::operator=(instance&& other) noexcept {
	if (this != &other) {
		delete std::exchange(d, std::exchange(other.d, nullptr));
	}

	return *this;
}

auto instance::handle() const -> void * {
	return d->instance_;
}

auto instance::make_surface(sdl::window const& window) -> surface const& {
	d->surface_.emplace(d->instance_, static_cast<SDL_Window *>(window.handle()));
	return *d->surface_;
}

auto instance::physical_devices() const -> std::vector<physical_device> {
	uint32_t device_count = 0;
	vkEnumeratePhysicalDevices(d->instance_, &device_count, nullptr);
	if (device_count == 0) {
		throw std::runtime_error("No Vulkan physical devices found.");
	}
	std::vector<VkPhysicalDevice> devices(device_count);
	vkEnumeratePhysicalDevices(d->instance_, &device_count, devices.data());

	std::vector<physical_device> physical_devices;
	physical_devices.reserve(devices.size());

	for (auto device : devices) {
		physical_devices.emplace_back(*this, static_cast<physical_device::physical_device_handle>(device));
	}

	return physical_devices;
}

} // namespace modern_vulkan