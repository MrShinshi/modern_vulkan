#include "instance.h"

#include <vulkan/vulkan.hpp>

struct modern_vulkan::instance_private {
};

modern_vulkan::instance::instance() {
	//auto app_info = vk::ApplicationInfo()
	//					.setPApplicationName("Vulkan C++ Windowed Program Template")
	//					.setApplicationVersion(1)
	//					.setPEngineName("LunarG SDK")
	//					.setEngineVersion(1)
	//					.setApiVersion(VK_API_VERSION_1_0);
	//auto instance_info{vk::InstanceCreateInfo()
	//					   .setFlags(vk::InstanceCreateFlags())
	//					   .setPApplicationInfo(&app_info)
	//					   .setEnabledExtensionCount(static_cast<uint32_t>(extensions.size()))
	//					   .setPpEnabledExtensionNames(extensions.data())
	//					   .setEnabledLayerCount(static_cast<uint32_t>(layers.size()))
	//					   .setPpEnabledLayerNames(layers.data())};
	//vk::createInstance(instance_info);
}

modern_vulkan::instance::~instance() {
}
