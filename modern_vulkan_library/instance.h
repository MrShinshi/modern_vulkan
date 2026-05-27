#pragma once
#include "modern_vulkan_library.h"

namespace modern_vulkan
{
struct instance_private;
struct MODERN_VULKAN_LIBRARY_EXPORT instance {

	instance();
	~instance();

private:
	instance_private* d;
};
} // namespace modern_vulkan