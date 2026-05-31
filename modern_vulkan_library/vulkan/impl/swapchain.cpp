#include <vulkan/swapchain.h>
#include <vulkan/surface.h>

#include <vulkan/logical_device.h>
#include <vulkan/mesh_renderer.h>
#include <vulkan/model_loader.h>
#include <vulkan/point_cloud_pipeline.h>
#include <vulkan/point_cloud_renderer.h>
#include <vulkan/physical_device.h>
#include <vulkan/stl_mesh.h>
#include <vulkan/point_cloud.h>

#include <vulkan/vulkan.hpp>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace modern_vulkan
{
namespace detail
{
auto choose_swapchain_surface_format(std::span<swapchain_surface_format const> formats) -> swapchain_surface_format {
	for (auto const& format : formats) {
		if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.color_space == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return format;
		}
	}

	return formats.front();
}

auto choose_swapchain_present_mode(std::span<int32_t const> present_modes) -> VkPresentModeKHR {
	for (auto const present_mode : present_modes) {
		if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
			return static_cast<VkPresentModeKHR>(present_mode);
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

auto choose_swapchain_extent(swapchain_surface_capabilities const& capabilities, rect requested_extent) -> VkExtent2D {
	if (capabilities.current_extent_defined && capabilities.current_extent.w > 0 && capabilities.current_extent.h > 0) {
		return VkExtent2D{
			.width = static_cast<uint32_t>(capabilities.current_extent.w),
			.height = static_cast<uint32_t>(capabilities.current_extent.h)};
	}

	return VkExtent2D{
		.width = std::clamp(
			static_cast<uint32_t>(std::max<std::size_t>(requested_extent.w, 1)),
			static_cast<uint32_t>(capabilities.min_image_extent.w),
			static_cast<uint32_t>(capabilities.max_image_extent.w)),
		.height = std::clamp(
			static_cast<uint32_t>(std::max<std::size_t>(requested_extent.h, 1)),
			static_cast<uint32_t>(capabilities.min_image_extent.h),
			static_cast<uint32_t>(capabilities.max_image_extent.h))};
}

auto unique_queue_family_indices(std::span<queue_family_support const> queue_families) -> std::vector<uint32_t> {
	std::vector<uint32_t> indices;
	indices.reserve(queue_families.size());

	for (auto const& queue_family : queue_families) {
		if (std::ranges::find(indices, queue_family.index) == indices.end()) {
			indices.push_back(queue_family.index);
		}
	}

	return indices;
}

auto has_stencil_component(VkFormat format) -> bool {
	return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

auto choose_depth_format(VkPhysicalDevice physical_device) -> VkFormat {
	constexpr std::array<VkFormat, 3> candidates{
		VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_D32_SFLOAT_S8_UINT,
		VK_FORMAT_D24_UNORM_S8_UINT};

	for (auto const candidate : candidates) {
		VkFormatProperties properties{};
		vkGetPhysicalDeviceFormatProperties(physical_device, candidate, &properties);
		if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0) {
			return candidate;
		}
	}

	throw std::runtime_error("No supported depth format found for swapchain depth attachment.");
}

auto find_memory_type(VkPhysicalDevice physical_device, uint32_t type_filter, VkMemoryPropertyFlags properties) -> uint32_t {
	VkPhysicalDeviceMemoryProperties memory_properties{};
	vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);

	for (uint32_t memory_type_index{}; memory_type_index < memory_properties.memoryTypeCount; ++memory_type_index) {
		if ((type_filter & (1u << memory_type_index)) != 0 &&
			(memory_properties.memoryTypes[memory_type_index].propertyFlags & properties) == properties) {
			return memory_type_index;
		}
	}

	throw std::runtime_error("Failed to find a suitable Vulkan memory type.");
}
} // namespace detail

struct swapchain_private {
	swapchain_private(logical_device const& logical_device, surface const& surface, rect requested_extent, void *old_swapchain_handle)
		: logical_device_(static_cast<VkDevice>(logical_device.handle())),
		  physical_device_(static_cast<VkPhysicalDevice>(logical_device.bound_physical_device().handle())) {
		if (logical_device_ == VK_NULL_HANDLE) {
			throw std::runtime_error("Logical device handle cannot be null.");
		}
		if (physical_device_ == VK_NULL_HANDLE) {
			throw std::runtime_error("Physical device handle cannot be null.");
		}

		auto const swapchain_support = logical_device.query_swapchain_support(surface);
		auto const surface_handle = surface.handle();
		if (surface_handle == nullptr) {
			throw std::runtime_error("Logical device must be bound to a surface to create a swapchain.");
		}

		auto const& surface_capabilities = swapchain_support.capabilities;
		auto const& surface_formats = swapchain_support.formats;
		auto const& present_modes = swapchain_support.present_modes;
		if (surface_formats.empty()) {
			throw std::runtime_error("No swapchain surface formats are available.");
		}
		if (present_modes.empty()) {
			throw std::runtime_error("No swapchain present modes are available.");
		}

		auto const surface_format = detail::choose_swapchain_surface_format(surface_formats);
		auto const present_mode = detail::choose_swapchain_present_mode(present_modes);
		auto const swapchain_extent = detail::choose_swapchain_extent(surface_capabilities, requested_extent);

		auto image_count = surface_capabilities.min_image_count + 1;
		if (surface_capabilities.max_image_count > 0 && image_count > surface_capabilities.max_image_count) {
			image_count = surface_capabilities.max_image_count;
		}

		auto queue_family_indices = detail::unique_queue_family_indices(logical_device.queue_families());
		if (queue_family_indices.empty()) {
			throw std::runtime_error("Logical device must expose at least one queue family to create a swapchain.");
		}

		VkSwapchainCreateInfoKHR create_info{
			.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.surface = reinterpret_cast<VkSurfaceKHR>(surface_handle),
			.minImageCount = image_count,
			.imageFormat = static_cast<VkFormat>(surface_format.format),
			.imageColorSpace = static_cast<VkColorSpaceKHR>(surface_format.color_space),
			.imageExtent = swapchain_extent,
			.imageArrayLayers = 1,
			.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
			.preTransform = static_cast<VkSurfaceTransformFlagBitsKHR>(surface_capabilities.current_transform),
			.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			.presentMode = present_mode,
			.clipped = VK_TRUE,
			.oldSwapchain = static_cast<VkSwapchainKHR>(old_swapchain_handle)};

		if (queue_family_indices.size() > 1) {
			create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			create_info.queueFamilyIndexCount = static_cast<uint32_t>(queue_family_indices.size());
			create_info.pQueueFamilyIndices = queue_family_indices.data();
		}

		auto const create_swapchain_result = vkCreateSwapchainKHR(logical_device_, &create_info, nullptr, &swapchain_);
		if (create_swapchain_result != VK_SUCCESS) {
			throw std::runtime_error("vkCreateSwapchainKHR failed with VkResult=" + std::to_string(static_cast<int32_t>(create_swapchain_result)) + ".");
		}

		uint32_t actual_image_count = 0;
		vkGetSwapchainImagesKHR(logical_device_, swapchain_, &actual_image_count, nullptr);
		images_.resize(actual_image_count);
		if (actual_image_count > 0) {
			vkGetSwapchainImagesKHR(logical_device_, swapchain_, &actual_image_count, images_.data());
		}

		image_format_ = static_cast<VkFormat>(surface_format.format);
		extent_ = swapchain_extent;

		image_views_.reserve(images_.size());
		for (auto const image : images_) {
			VkImageViewCreateInfo image_view_create_info{
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.image = image,
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
				.format = image_format_,
				.components = {
					.r = VK_COMPONENT_SWIZZLE_IDENTITY,
					.g = VK_COMPONENT_SWIZZLE_IDENTITY,
					.b = VK_COMPONENT_SWIZZLE_IDENTITY,
					.a = VK_COMPONENT_SWIZZLE_IDENTITY},
				.subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1}};

			VkImageView image_view = VK_NULL_HANDLE;
			if (vkCreateImageView(logical_device_, &image_view_create_info, nullptr, &image_view) != VK_SUCCESS) {
				throw std::runtime_error("vkCreateImageView failed for swapchain image.");
			}

			image_views_.push_back(image_view);
		}

		depth_format_ = detail::choose_depth_format(physical_device_);

		VkImageCreateInfo depth_image_create_info{
			.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			.imageType = VK_IMAGE_TYPE_2D,
			.format = depth_format_,
			.extent = {
				.width = extent_.width,
				.height = extent_.height,
				.depth = 1},
			.mipLevels = 1,
			.arrayLayers = 1,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.tiling = VK_IMAGE_TILING_OPTIMAL,
			.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};

		if (vkCreateImage(logical_device_, &depth_image_create_info, nullptr, &depth_image_) != VK_SUCCESS) {
			throw std::runtime_error("vkCreateImage failed for swapchain depth image.");
		}

		VkMemoryRequirements depth_image_memory_requirements{};
		vkGetImageMemoryRequirements(logical_device_, depth_image_, &depth_image_memory_requirements);

		VkMemoryAllocateInfo depth_memory_allocate_info{
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.allocationSize = depth_image_memory_requirements.size,
			.memoryTypeIndex = detail::find_memory_type(
				physical_device_,
				depth_image_memory_requirements.memoryTypeBits,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)};

		if (vkAllocateMemory(logical_device_, &depth_memory_allocate_info, nullptr, &depth_image_memory_) != VK_SUCCESS) {
			throw std::runtime_error("vkAllocateMemory failed for swapchain depth image.");
		}

		if (vkBindImageMemory(logical_device_, depth_image_, depth_image_memory_, 0) != VK_SUCCESS) {
			throw std::runtime_error("vkBindImageMemory failed for swapchain depth image.");
		}

		VkImageAspectFlags depth_aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT;
		if (detail::has_stencil_component(depth_format_)) {
			depth_aspect_mask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}

		VkImageViewCreateInfo depth_image_view_create_info{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = depth_image_,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = depth_format_,
			.components = {
				.r = VK_COMPONENT_SWIZZLE_IDENTITY,
				.g = VK_COMPONENT_SWIZZLE_IDENTITY,
				.b = VK_COMPONENT_SWIZZLE_IDENTITY,
				.a = VK_COMPONENT_SWIZZLE_IDENTITY},
			.subresourceRange = {.aspectMask = depth_aspect_mask, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1}};

		if (vkCreateImageView(logical_device_, &depth_image_view_create_info, nullptr, &depth_image_view_) != VK_SUCCESS) {
			throw std::runtime_error("vkCreateImageView failed for swapchain depth image.");
		}

		VkAttachmentDescription color_attachment{
			.flags = 0,
			.format = image_format_,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR};
		VkAttachmentDescription depth_attachment{
			.flags = 0,
			.format = depth_format_,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

		VkAttachmentReference color_attachment_reference{
			.attachment = 0,
			.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
		VkAttachmentReference depth_attachment_reference{
			.attachment = 1,
			.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

		VkSubpassDescription subpass_description{
			.flags = 0,
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.colorAttachmentCount = 1,
			.pColorAttachments = &color_attachment_reference,
			.pDepthStencilAttachment = &depth_attachment_reference};

		VkSubpassDependency subpass_dependency{
			.srcSubpass = VK_SUBPASS_EXTERNAL,
			.dstSubpass = 0,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
			.srcAccessMask = 0,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			.dependencyFlags = 0};

		std::array<VkAttachmentDescription, 2> attachments{color_attachment, depth_attachment};
		VkRenderPassCreateInfo render_pass_create_info{
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.attachmentCount = static_cast<uint32_t>(attachments.size()),
			.pAttachments = attachments.data(),
			.subpassCount = 1,
			.pSubpasses = &subpass_description,
			.dependencyCount = 1,
			.pDependencies = &subpass_dependency};

		if (vkCreateRenderPass(logical_device_, &render_pass_create_info, nullptr, &render_pass_) != VK_SUCCESS) {
			throw std::runtime_error("vkCreateRenderPass failed for swapchain render pass.");
		}

		framebuffers_.reserve(image_views_.size());
		for (auto const image_view : image_views_) {
			std::array<VkImageView, 2> framebuffer_attachments{image_view, depth_image_view_};
			VkFramebufferCreateInfo framebuffer_create_info{
				.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
				.renderPass = render_pass_,
				.attachmentCount = static_cast<uint32_t>(framebuffer_attachments.size()),
				.pAttachments = framebuffer_attachments.data(),
				.width = extent_.width,
				.height = extent_.height,
				.layers = 1};

			VkFramebuffer framebuffer = VK_NULL_HANDLE;
			if (vkCreateFramebuffer(logical_device_, &framebuffer_create_info, nullptr, &framebuffer) != VK_SUCCESS) {
				throw std::runtime_error("vkCreateFramebuffer failed for swapchain image view.");
			}

			framebuffers_.push_back(framebuffer);
		}
	}

	~swapchain_private() {
		for (auto const framebuffer : framebuffers_) {
			if (logical_device_ != VK_NULL_HANDLE && framebuffer != VK_NULL_HANDLE) {
				vkDestroyFramebuffer(logical_device_, framebuffer, nullptr);
			}
		}

		if (logical_device_ != VK_NULL_HANDLE && render_pass_ != VK_NULL_HANDLE) {
			vkDestroyRenderPass(logical_device_, render_pass_, nullptr);
		}

		if (logical_device_ != VK_NULL_HANDLE && depth_image_view_ != VK_NULL_HANDLE) {
			vkDestroyImageView(logical_device_, depth_image_view_, nullptr);
		}

		if (logical_device_ != VK_NULL_HANDLE && depth_image_ != VK_NULL_HANDLE) {
			vkDestroyImage(logical_device_, depth_image_, nullptr);
		}

		if (logical_device_ != VK_NULL_HANDLE && depth_image_memory_ != VK_NULL_HANDLE) {
			vkFreeMemory(logical_device_, depth_image_memory_, nullptr);
		}

		for (auto const image_view : image_views_) {
			if (logical_device_ != VK_NULL_HANDLE && image_view != VK_NULL_HANDLE) {
				vkDestroyImageView(logical_device_, image_view, nullptr);
			}
		}

		if (logical_device_ != VK_NULL_HANDLE && swapchain_ != VK_NULL_HANDLE) {
			vkDestroySwapchainKHR(logical_device_, swapchain_, nullptr);
		}
	}

	VkDevice logical_device_ = VK_NULL_HANDLE;
	VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
	VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
	VkFormat image_format_ = VK_FORMAT_UNDEFINED;
	VkExtent2D extent_{};
	std::vector<VkImage> images_;
	std::vector<VkImageView> image_views_;
	VkImage depth_image_ = VK_NULL_HANDLE;
	VkDeviceMemory depth_image_memory_ = VK_NULL_HANDLE;
	VkImageView depth_image_view_ = VK_NULL_HANDLE;
	VkFormat depth_format_ = VK_FORMAT_UNDEFINED;
	VkRenderPass render_pass_ = VK_NULL_HANDLE;
	std::vector<VkFramebuffer> framebuffers_;
};

swapchain::swapchain(logical_device const& logical_device, surface const& surface, rect extent, void *old_swapchain)
	: d(new swapchain_private(logical_device, surface, extent, old_swapchain)) {
}

swapchain::~swapchain() {
	delete d;
}

swapchain::swapchain(swapchain&& other) noexcept : d(other.d) {
	other.d = nullptr;
}

swapchain& swapchain::operator=(swapchain&& other) noexcept {
	if (this != &other) {
		delete std::exchange(d, std::exchange(other.d, nullptr));
	}

	return *this;
}

auto swapchain::extent() const -> rect {
	if (d == nullptr) {
		return {};
	}

	return rect{.w = d->extent_.width, .h = d->extent_.height};
}

auto swapchain::handle() const -> void * {
	if (d == nullptr) {
		return nullptr;
	}

	return static_cast<void *>(d->swapchain_);
}

auto swapchain::image_format() const -> int32_t {
	if (d == nullptr) {
		return static_cast<int32_t>(VK_FORMAT_UNDEFINED);
	}

	return static_cast<int32_t>(d->image_format_);
}

auto swapchain::image_count() const -> uint32_t {
	if (d == nullptr) {
		return 0;
	}

	return static_cast<uint32_t>(d->images_.size());
}

auto swapchain::image_view_count() const -> uint32_t {
	if (d == nullptr) {
		return 0;
	}

	return static_cast<uint32_t>(d->image_views_.size());
}

auto swapchain::image_view(uint32_t index) const -> void * {
	if (d == nullptr || index >= d->image_views_.size()) {
		return nullptr;
	}

	return static_cast<void *>(d->image_views_[index]);
}

auto swapchain::render_pass() const -> void * {
	if (d == nullptr) {
		return nullptr;
	}

	return static_cast<void *>(d->render_pass_);
}

auto swapchain::framebuffer_count() const -> uint32_t {
	if (d == nullptr) {
		return 0;
	}

	return static_cast<uint32_t>(d->framebuffers_.size());
}

auto swapchain::framebuffer(uint32_t index) const -> void * {
	if (d == nullptr || index >= d->framebuffers_.size()) {
		return nullptr;
	}

	return static_cast<void *>(d->framebuffers_[index]);
}
} // namespace modern_vulkan

namespace modern_vulkan
{
namespace pipeline_detail
{
auto read_binary_file(std::filesystem::path const& path) -> std::vector<char> {
	std::ifstream file(path, std::ios::ate | std::ios::binary);
	if (!file.is_open()) {
		throw std::runtime_error("Failed to open shader file: " + path.string());
	}

	auto const size = file.tellg();
	if (size <= 0) {
		throw std::runtime_error("Shader file is empty: " + path.string());
	}

	std::vector<char> bytes(static_cast<size_t>(size));
	file.seekg(0);
	file.read(bytes.data(), static_cast<std::streamsize>(size));
	if (!file) {
		throw std::runtime_error("Failed to read shader file: " + path.string());
	}

	return bytes;
}

auto create_shader_module(VkDevice device, std::vector<char> const& code) -> VkShaderModule {
	VkShaderModuleCreateInfo create_info{
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = code.size(),
		.pCode = reinterpret_cast<uint32_t const *>(code.data())};

	VkShaderModule shader_module = VK_NULL_HANDLE;
	if (vkCreateShaderModule(device, &create_info, nullptr, &shader_module) != VK_SUCCESS) {
		throw std::runtime_error("vkCreateShaderModule failed.");
	}

	return shader_module;
}
} // namespace pipeline_detail

struct point_cloud_pipeline_private {
	point_cloud_pipeline_private(
		logical_device const& logical_device,
		swapchain const& swapchain,
		std::filesystem::path const& vertex_shader_path,
		std::filesystem::path const& fragment_shader_path,
		primitive_topology topology,
		polygon_mode polygon)
		: logical_device_(static_cast<VkDevice>(logical_device.handle())) {
		if (logical_device_ == VK_NULL_HANDLE) {
			throw std::runtime_error("Logical device handle cannot be null.");
		}

		auto const render_pass = static_cast<VkRenderPass>(swapchain.render_pass());
		if (render_pass == VK_NULL_HANDLE) {
			throw std::runtime_error("Swapchain render pass cannot be null.");
		}

		auto const vertex_shader_code = pipeline_detail::read_binary_file(vertex_shader_path);
		auto const fragment_shader_code = pipeline_detail::read_binary_file(fragment_shader_path);
		VkShaderModule const vertex_shader_module = pipeline_detail::create_shader_module(logical_device_, vertex_shader_code);
		VkShaderModule const fragment_shader_module = pipeline_detail::create_shader_module(logical_device_, fragment_shader_code);

		try {
			VkPushConstantRange push_constant_range{
				.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				.offset = 0,
				.size = static_cast<uint32_t>((sizeof(float) * 16) + (sizeof(float) * 4))};
			VkPipelineLayoutCreateInfo pipeline_layout_create_info{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
				.pushConstantRangeCount = 1,
				.pPushConstantRanges = &push_constant_range};
			if (vkCreatePipelineLayout(logical_device_, &pipeline_layout_create_info, nullptr, &pipeline_layout_) != VK_SUCCESS) {
				throw std::runtime_error("vkCreatePipelineLayout failed for point cloud pipeline.");
			}

			VkPipelineShaderStageCreateInfo vertex_shader_stage_create_info{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = VK_SHADER_STAGE_VERTEX_BIT,
				.module = vertex_shader_module,
				.pName = "main"};
			VkPipelineShaderStageCreateInfo fragment_shader_stage_create_info{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
				.module = fragment_shader_module,
				.pName = "main"};
			std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages{
				vertex_shader_stage_create_info,
				fragment_shader_stage_create_info};

			VkVertexInputBindingDescription binding_description{
				.binding = 0,
				.stride = static_cast<uint32_t>(sizeof(point_vertex)),
				.inputRate = VK_VERTEX_INPUT_RATE_VERTEX};
			std::array<VkVertexInputAttributeDescription, 3> attribute_descriptions{
				VkVertexInputAttributeDescription{
					.location = 0,
					.binding = 0,
					.format = VK_FORMAT_R32G32B32_SFLOAT,
					.offset = static_cast<uint32_t>(offsetof(point_vertex, position))},
				VkVertexInputAttributeDescription{
					.location = 1,
					.binding = 0,
					.format = VK_FORMAT_R32G32B32_SFLOAT,
					.offset = static_cast<uint32_t>(offsetof(point_vertex, color))},
				VkVertexInputAttributeDescription{
					.location = 2,
					.binding = 0,
					.format = VK_FORMAT_R32G32B32_SFLOAT,
					.offset = static_cast<uint32_t>(offsetof(point_vertex, normal))}};
			VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
				.vertexBindingDescriptionCount = 1,
				.pVertexBindingDescriptions = &binding_description,
				.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size()),
				.pVertexAttributeDescriptions = attribute_descriptions.data()};

			VkPipelineInputAssemblyStateCreateInfo input_assembly_state_create_info{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
				.topology = topology == primitive_topology::triangle_list ? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST : VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
				.primitiveRestartEnable = VK_FALSE};

			auto const swapchain_extent = swapchain.extent();
			VkViewport viewport{
				.x = 0.0f,
				.y = 0.0f,
				.width = static_cast<float>(swapchain_extent.w),
				.height = static_cast<float>(swapchain_extent.h),
				.minDepth = 0.0f,
				.maxDepth = 1.0f};
			VkRect2D scissor{
				.offset = {.x = 0, .y = 0},
				.extent = {
					.width = static_cast<uint32_t>(swapchain_extent.w),
					.height = static_cast<uint32_t>(swapchain_extent.h)}};
			VkPipelineViewportStateCreateInfo viewport_state_create_info{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
				.viewportCount = 1,
				.pViewports = &viewport,
				.scissorCount = 1,
				.pScissors = &scissor};

			VkPipelineRasterizationStateCreateInfo rasterization_state_create_info{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
				.depthClampEnable = VK_FALSE,
				.rasterizerDiscardEnable = VK_FALSE,
				.polygonMode = polygon == polygon_mode::line ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL,
				.cullMode = VK_CULL_MODE_NONE,
				.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
				.depthBiasEnable = VK_FALSE,
				.lineWidth = 1.0f};

			VkPipelineMultisampleStateCreateInfo multisample_state_create_info{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
				.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
				.sampleShadingEnable = VK_FALSE};

			VkPipelineDepthStencilStateCreateInfo depth_stencil_state_create_info{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
				.depthTestEnable = VK_TRUE,
				.depthWriteEnable = VK_TRUE,
				.depthCompareOp = VK_COMPARE_OP_LESS,
				.depthBoundsTestEnable = VK_FALSE,
				.stencilTestEnable = VK_FALSE};

			VkPipelineColorBlendAttachmentState color_blend_attachment_state{
				.blendEnable = VK_FALSE,
				.srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
				.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
				.colorBlendOp = VK_BLEND_OP_ADD,
				.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
				.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
				.alphaBlendOp = VK_BLEND_OP_ADD,
				.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};
			VkPipelineColorBlendStateCreateInfo color_blend_state_create_info{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
				.logicOpEnable = VK_FALSE,
				.attachmentCount = 1,
				.pAttachments = &color_blend_attachment_state};

			VkGraphicsPipelineCreateInfo pipeline_create_info{
				.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
				.stageCount = static_cast<uint32_t>(shader_stages.size()),
				.pStages = shader_stages.data(),
				.pVertexInputState = &vertex_input_state_create_info,
				.pInputAssemblyState = &input_assembly_state_create_info,
				.pViewportState = &viewport_state_create_info,
				.pRasterizationState = &rasterization_state_create_info,
				.pMultisampleState = &multisample_state_create_info,
				.pDepthStencilState = &depth_stencil_state_create_info,
				.pColorBlendState = &color_blend_state_create_info,
				.layout = pipeline_layout_,
				.renderPass = render_pass,
				.subpass = 0};

			if (vkCreateGraphicsPipelines(logical_device_, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &pipeline_) != VK_SUCCESS) {
				throw std::runtime_error("vkCreateGraphicsPipelines failed for point cloud pipeline.");
			}
		}
		catch (...) {
			vkDestroyShaderModule(logical_device_, fragment_shader_module, nullptr);
			vkDestroyShaderModule(logical_device_, vertex_shader_module, nullptr);
			throw;
		}

		vkDestroyShaderModule(logical_device_, fragment_shader_module, nullptr);
		vkDestroyShaderModule(logical_device_, vertex_shader_module, nullptr);
	}

	~point_cloud_pipeline_private() {
		if (logical_device_ != VK_NULL_HANDLE && pipeline_ != VK_NULL_HANDLE) {
			vkDestroyPipeline(logical_device_, pipeline_, nullptr);
		}

		if (logical_device_ != VK_NULL_HANDLE && pipeline_layout_ != VK_NULL_HANDLE) {
			vkDestroyPipelineLayout(logical_device_, pipeline_layout_, nullptr);
		}
	}

	VkDevice logical_device_ = VK_NULL_HANDLE;
	VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
	VkPipeline pipeline_ = VK_NULL_HANDLE;
};

point_cloud_pipeline::point_cloud_pipeline(
	logical_device const& logical_device,
	swapchain const& swapchain,
	std::filesystem::path const& vertex_shader_path,
	std::filesystem::path const& fragment_shader_path,
	primitive_topology topology,
	polygon_mode polygon)
	: d(new point_cloud_pipeline_private(logical_device, swapchain, vertex_shader_path, fragment_shader_path, topology, polygon)) {
}

point_cloud_pipeline::~point_cloud_pipeline() {
	delete d;
}

point_cloud_pipeline::point_cloud_pipeline(point_cloud_pipeline&& other) noexcept : d(other.d) {
	other.d = nullptr;
}

point_cloud_pipeline& point_cloud_pipeline::operator=(point_cloud_pipeline&& other) noexcept {
	if (this != &other) {
		delete std::exchange(d, std::exchange(other.d, nullptr));
	}

	return *this;
}

auto point_cloud_pipeline::handle() const -> void * {
	if (d == nullptr) {
		return nullptr;
	}

	return static_cast<void *>(d->pipeline_);
}

auto point_cloud_pipeline::layout() const -> void * {
	if (d == nullptr) {
		return nullptr;
	}

	return static_cast<void *>(d->pipeline_layout_);
}
} // namespace modern_vulkan

namespace modern_vulkan
{
namespace stl_detail
{
struct point_position_key {
	uint32_t x;
	uint32_t y;
	uint32_t z;

	auto operator==(point_position_key const& other) const -> bool = default;
};

struct point_position_key_hash {
	auto operator()(point_position_key const& value) const noexcept -> size_t {
		auto hash = static_cast<size_t>(value.x);
		hash ^= static_cast<size_t>(value.y) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
		hash ^= static_cast<size_t>(value.z) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
		return hash;
	}
};

struct accumulated_normal {
	std::array<float, 3> value{0.0f, 0.0f, 0.0f};
};

auto make_point_position_key(std::array<float, 3> const& position) -> point_position_key {
	return point_position_key{
		.x = std::bit_cast<uint32_t>(position[0]),
		.y = std::bit_cast<uint32_t>(position[1]),
		.z = std::bit_cast<uint32_t>(position[2])};
}

auto normalize_normal(std::array<float, 3> const& normal) -> std::array<float, 3> {
	auto const length = std::sqrt(
		normal[0] * normal[0] +
		normal[1] * normal[1] +
		normal[2] * normal[2]);
	if (length <= 1.0e-6f) {
		return {0.0f, 0.0f, 1.0f};
	}

	return {
		normal[0] / length,
		normal[1] / length,
		normal[2] / length};
}

void accumulate_vertex_normal(
	std::unordered_map<point_position_key, accumulated_normal, point_position_key_hash>& accumulated_normals,
	std::array<float, 3> const& position,
	std::array<float, 3> const& normal) {
	auto& accumulated = accumulated_normals[make_point_position_key(position)].value;
	auto const normalized_normal = normalize_normal(normal);
	for (size_t component_index{}; component_index < accumulated.size(); ++component_index) {
		accumulated[component_index] += normalized_normal[component_index];
	}
}

void apply_smoothed_normals(
	model_data& mesh,
	std::unordered_map<point_position_key, accumulated_normal, point_position_key_hash> const& accumulated_normals) {
	for (auto& vertex : mesh.vertices) {
		auto const normal_iterator = accumulated_normals.find(make_point_position_key(vertex.position));
		if (normal_iterator == accumulated_normals.end()) {
			continue;
		}

		vertex.normal = normalize_normal(normal_iterator->second.value);
	}
}

void update_bounds(modern_vulkan::bounds& value_bounds, std::array<float, 3> const& position) {
	for (size_t component_index{}; component_index < position.size(); ++component_index) {
		value_bounds.min[component_index] = std::min(value_bounds.min[component_index], position[component_index]);
		value_bounds.max[component_index] = std::max(value_bounds.max[component_index], position[component_index]);
	}
}

void append_unique_point(
	model_data& point_cloud,
	std::unordered_set<point_position_key, point_position_key_hash>& unique_positions,
	std::array<float, 3> const& position,
	std::array<float, 3> const& color,
	std::array<float, 3> const& normal) {
	if (!unique_positions.insert(make_point_position_key(position)).second) {
		return;
	}

	point_cloud.vertices.push_back(point_vertex{
		.position = position,
		.color = color,
		.normal = normal});
	update_bounds(point_cloud.bounds, position);
}

void append_mesh_vertex(
	model_data& mesh,
	std::array<float, 3> const& position,
	std::array<float, 3> const& color,
	std::array<float, 3> const& normal) {
	mesh.vertices.push_back(point_vertex{
		.position = position,
		.color = color,
		.normal = normal});
	update_bounds(mesh.bounds, position);
}

void ensure_read_success(std::istream& stream, std::filesystem::path const& path, char const *message) {
	if (!stream) {
		throw std::runtime_error(std::string(message) + path.string());
	}
}

auto default_bounds() -> modern_vulkan::bounds {
	return modern_vulkan::bounds{
		.min = {
			std::numeric_limits<float>::max(),
			std::numeric_limits<float>::max(),
			std::numeric_limits<float>::max()},
		.max = {std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()}};
}

auto is_binary_stl(std::filesystem::path const& path) -> bool {
	std::ifstream stream(path, std::ios::binary | std::ios::ate);
	if (!stream.is_open()) {
		throw std::runtime_error("Failed to open STL file: " + path.string());
	}

	auto const file_size = stream.tellg();
	if (file_size < static_cast<std::streamoff>(84)) {
		throw std::runtime_error("STL file is too small: " + path.string());
	}

	stream.seekg(80);
	uint32_t triangle_count{};
	stream.read(reinterpret_cast<char *>(&triangle_count), sizeof(triangle_count));
	ensure_read_success(stream, path, "Failed to read STL triangle count: ");

	auto const expected_size = static_cast<uint64_t>(84) + static_cast<uint64_t>(triangle_count) * static_cast<uint64_t>(50);
	return expected_size == static_cast<uint64_t>(file_size);
}

auto load_binary_stl_point_cloud(std::filesystem::path const& path, std::array<float, 3> const& color) -> model_data {
	std::ifstream stream(path, std::ios::binary);
	if (!stream.is_open()) {
		throw std::runtime_error("Failed to open STL file: " + path.string());
	}

	std::array<char, 80> header{};
	stream.read(header.data(), static_cast<std::streamsize>(header.size()));
	ensure_read_success(stream, path, "Failed to read STL header: ");

	uint32_t triangle_count{};
	stream.read(reinterpret_cast<char *>(&triangle_count), sizeof(triangle_count));
	ensure_read_success(stream, path, "Failed to read STL triangle count: ");

	auto point_cloud = model_data{.bounds = default_bounds()};
	point_cloud.vertices.reserve(static_cast<size_t>(triangle_count) * 3);
	std::unordered_set<point_position_key, point_position_key_hash> unique_positions;
	unique_positions.reserve(static_cast<size_t>(triangle_count) * 3);

	for (uint32_t triangle_index{}; triangle_index < triangle_count; ++triangle_index) {
		std::array<float, 3> normal{};
		stream.read(reinterpret_cast<char *>(normal.data()), static_cast<std::streamsize>(sizeof(normal)));
		ensure_read_success(stream, path, "Failed to read STL triangle normal: ");

		for (uint32_t vertex_index{}; vertex_index < 3; ++vertex_index) {
			std::array<float, 3> position{};
			stream.read(reinterpret_cast<char *>(position.data()), static_cast<std::streamsize>(sizeof(position)));
			ensure_read_success(stream, path, "Failed to read STL triangle vertex: ");
			append_unique_point(point_cloud, unique_positions, position, color, normal);
		}

		uint16_t attribute_byte_count{};
		stream.read(reinterpret_cast<char *>(&attribute_byte_count), sizeof(attribute_byte_count));
		ensure_read_success(stream, path, "Failed to read STL triangle attribute byte count: ");
	}

	return point_cloud;
}

auto load_binary_stl_mesh(std::filesystem::path const& path, std::array<float, 3> const& color) -> model_data {
	std::ifstream stream(path, std::ios::binary);
	if (!stream.is_open()) {
		throw std::runtime_error("Failed to open STL file: " + path.string());
	}

	std::array<char, 80> header{};
	stream.read(header.data(), static_cast<std::streamsize>(header.size()));
	ensure_read_success(stream, path, "Failed to read STL header: ");

	uint32_t triangle_count{};
	stream.read(reinterpret_cast<char *>(&triangle_count), sizeof(triangle_count));
	ensure_read_success(stream, path, "Failed to read STL triangle count: ");

	auto mesh = model_data{.bounds = default_bounds()};
	mesh.vertices.reserve(static_cast<size_t>(triangle_count) * 3);
	std::unordered_map<point_position_key, accumulated_normal, point_position_key_hash> accumulated_normals;
	accumulated_normals.reserve(static_cast<size_t>(triangle_count) * 3);

	for (uint32_t triangle_index{}; triangle_index < triangle_count; ++triangle_index) {
		std::array<float, 3> normal{};
		stream.read(reinterpret_cast<char *>(normal.data()), static_cast<std::streamsize>(sizeof(normal)));
		ensure_read_success(stream, path, "Failed to read STL triangle normal: ");

		for (uint32_t vertex_index{}; vertex_index < 3; ++vertex_index) {
			std::array<float, 3> position{};
			stream.read(reinterpret_cast<char *>(position.data()), static_cast<std::streamsize>(sizeof(position)));
			ensure_read_success(stream, path, "Failed to read STL triangle vertex: ");
			append_mesh_vertex(mesh, position, color, normal);
			accumulate_vertex_normal(accumulated_normals, position, normal);
		}

		uint16_t attribute_byte_count{};
		stream.read(reinterpret_cast<char *>(&attribute_byte_count), sizeof(attribute_byte_count));
		ensure_read_success(stream, path, "Failed to read STL triangle attribute byte count: ");
	}

	apply_smoothed_normals(mesh, accumulated_normals);

	return mesh;
}

auto load_ascii_stl_point_cloud(std::filesystem::path const& path, std::array<float, 3> const& color) -> model_data {
	std::ifstream stream(path);
	if (!stream.is_open()) {
		throw std::runtime_error("Failed to open STL file: " + path.string());
	}

	auto point_cloud = model_data{.bounds = default_bounds()};
	std::unordered_set<point_position_key, point_position_key_hash> unique_positions;
	std::string token;
	std::array<float, 3> current_normal{0.0f, 0.0f, 1.0f};

	while (stream >> token) {
		if (token == "facet") {
			std::string normal_token;
			stream >> normal_token >> current_normal[0] >> current_normal[1] >> current_normal[2];
			ensure_read_success(stream, path, "Failed to parse ASCII STL facet normal: ");
			continue;
		}

		if (token != "vertex") {
			continue;
		}

		std::array<float, 3> position{};
		stream >> position[0] >> position[1] >> position[2];
		ensure_read_success(stream, path, "Failed to parse ASCII STL vertex: ");
		append_unique_point(point_cloud, unique_positions, position, color, current_normal);
	}

	return point_cloud;
}

auto load_ascii_stl_mesh(std::filesystem::path const& path, std::array<float, 3> const& color) -> model_data {
	std::ifstream stream(path);
	if (!stream.is_open()) {
		throw std::runtime_error("Failed to open STL file: " + path.string());
	}

	auto mesh = model_data{.bounds = default_bounds()};
	std::string token;
	std::array<float, 3> current_normal{0.0f, 0.0f, 1.0f};
	std::unordered_map<point_position_key, accumulated_normal, point_position_key_hash> accumulated_normals;

	while (stream >> token) {
		if (token == "facet") {
			std::string normal_token;
			stream >> normal_token >> current_normal[0] >> current_normal[1] >> current_normal[2];
			ensure_read_success(stream, path, "Failed to parse ASCII STL facet normal: ");
			continue;
		}

		if (token != "vertex") {
			continue;
		}

		std::array<float, 3> position{};
		stream >> position[0] >> position[1] >> position[2];
		ensure_read_success(stream, path, "Failed to parse ASCII STL vertex: ");
		append_mesh_vertex(mesh, position, color, current_normal);
		accumulate_vertex_normal(accumulated_normals, position, current_normal);
	}

	apply_smoothed_normals(mesh, accumulated_normals);

	return mesh;
}
} // namespace stl_detail

auto load_stl_point_cloud(std::filesystem::path const& path, std::array<float, 3> const& color) -> model_data {
	if (!std::filesystem::exists(path)) {
		throw std::runtime_error("STL file does not exist: " + path.string());
	}

	auto point_cloud = stl_detail::is_binary_stl(path)
						   ? stl_detail::load_binary_stl_point_cloud(path, color)
						   : stl_detail::load_ascii_stl_point_cloud(path, color);

	if (point_cloud.vertices.empty()) {
		throw std::runtime_error("No point data was extracted from STL file: " + path.string());
	}

	return point_cloud;
}

auto load_stl_mesh(std::filesystem::path const& path, std::array<float, 3> const& color) -> model_data {
	if (!std::filesystem::exists(path)) {
		throw std::runtime_error("STL file does not exist: " + path.string());
	}

	auto mesh = stl_detail::is_binary_stl(path)
					? stl_detail::load_binary_stl_mesh(path, color)
					: stl_detail::load_ascii_stl_mesh(path, color);

	if (mesh.vertices.empty()) {
		throw std::runtime_error("No mesh data was extracted from STL file: " + path.string());
	}

	return mesh;
}

namespace loader_detail
{
struct obj_face_vertex {
	int32_t position_index = 0;
	int32_t normal_index = 0;
};

struct obj_triangle {
	std::array<obj_face_vertex, 3> vertices{};
	std::array<float, 3> color{0.0f, 0.0f, 0.0f};
	bool has_color = false;
};

struct obj_material {
	std::array<float, 3> diffuse_color{1.0f, 1.0f, 1.0f};
	bool has_diffuse_color = false;
};

auto trim_ascii(std::string const& value) -> std::string {
	auto const first = value.find_first_not_of(" \t\r\n");
	if (first == std::string::npos) {
		return {};
	}

	auto const last = value.find_last_not_of(" \t\r\n");
	return value.substr(first, last - first + 1);
}

auto normalize_color_component(float component) -> float {
	if (component > 1.0f) {
		return std::clamp(component / 255.0f, 0.0f, 1.0f);
	}

	return std::clamp(component, 0.0f, 1.0f);
}

auto normalize_color(std::array<float, 3> color) -> std::array<float, 3> {
	for (auto& component : color) {
		component = normalize_color_component(component);
	}

	return color;
}

auto parse_obj_material_library(std::filesystem::path const& obj_path, std::string const& library_name) -> std::unordered_map<std::string, obj_material> {
	auto const material_path = obj_path.parent_path() / trim_ascii(library_name);
	std::ifstream stream(material_path);
	if (!stream.is_open()) {
		return {};
	}

	std::unordered_map<std::string, obj_material> materials;
	std::string line;
	std::string current_material_name;

	while (std::getline(stream, line)) {
		auto const trimmed = trim_ascii(line);
		if (trimmed.empty() || trimmed[0] == '#') {
			continue;
		}

		std::istringstream line_stream(trimmed);
		std::string prefix;
		line_stream >> prefix;

		if (prefix == "newmtl") {
			line_stream >> current_material_name;
			if (!current_material_name.empty()) {
				materials.try_emplace(current_material_name, obj_material{});
			}
			continue;
		}

		if (prefix == "Kd" && !current_material_name.empty()) {
			std::array<float, 3> diffuse_color{};
			line_stream >> diffuse_color[0] >> diffuse_color[1] >> diffuse_color[2];
			if (line_stream) {
				auto& material = materials[current_material_name];
				material.diffuse_color = normalize_color(diffuse_color);
				material.has_diffuse_color = true;
			}
		}
	}

	return materials;
}

auto normalize_index(int32_t index, size_t count) -> size_t {
	if (index > 0) {
		return static_cast<size_t>(index - 1);
	}
	if (index < 0) {
		return static_cast<size_t>(static_cast<int32_t>(count) + index);
	}

	throw std::runtime_error("OBJ index 0 is invalid.");
}

auto parse_obj_face_vertex(std::string const& token) -> obj_face_vertex {
	obj_face_vertex result;
	std::array<std::string, 3> parts{};
	size_t part_index = 0;
	size_t start = 0;

	for (size_t index{}; index <= token.size() && part_index < parts.size(); ++index) {
		if (index == token.size() || token[index] == '/') {
			parts[part_index++] = token.substr(start, index - start);
			start = index + 1;
		}
	}

	if (!parts[0].empty()) {
		result.position_index = std::stoi(parts[0]);
		if (part_index >= 3 && !parts[2].empty()) {
			result.normal_index = std::stoi(parts[2]);
		}
	}

	return result;
}

auto compute_face_normal(
	std::array<float, 3> const& a,
	std::array<float, 3> const& b,
	std::array<float, 3> const& c) -> std::array<float, 3> {
	return stl_detail::normalize_normal({((b[1] - a[1]) * (c[2] - a[2])) - ((b[2] - a[2]) * (c[1] - a[1])),
										 ((b[2] - a[2]) * (c[0] - a[0])) - ((b[0] - a[0]) * (c[2] - a[2])),
										 ((b[0] - a[0]) * (c[1] - a[1])) - ((b[1] - a[1]) * (c[0] - a[0]))});
}

void append_model_vertex(model_data& model, std::array<float, 3> const& position, std::array<float, 3> const& color, std::array<float, 3> const& normal) {
	model.vertices.push_back(point_vertex{
		.position = position,
		.color = normalize_color(color),
		.normal = stl_detail::normalize_normal(normal)});
	stl_detail::update_bounds(model.bounds, position);
}

auto build_model_from_triangles(
	std::vector<std::array<float, 3>> const& positions,
	std::vector<std::optional<std::array<float, 3>>> const& position_colors,
	std::vector<std::array<float, 3>> const& normals,
	std::vector<obj_triangle> const& triangles,
	model_representation representation,
	std::array<float, 3> const& color) -> model_data {
	auto model = model_data{.bounds = stl_detail::default_bounds()};
	if (representation == model_representation::mesh) {
		model.vertices.reserve(triangles.size() * 3);
		std::unordered_map<stl_detail::point_position_key, stl_detail::accumulated_normal, stl_detail::point_position_key_hash> accumulated_normals;
		accumulated_normals.reserve(triangles.size() * 3);

		for (auto const& triangle : triangles) {
			auto const a_index = normalize_index(triangle.vertices[0].position_index, positions.size());
			auto const b_index = normalize_index(triangle.vertices[1].position_index, positions.size());
			auto const c_index = normalize_index(triangle.vertices[2].position_index, positions.size());
			auto const& a = positions[a_index];
			auto const& b = positions[b_index];
			auto const& c = positions[c_index];
			auto const face_normal = (triangle.vertices[0].normal_index != 0 && triangle.vertices[1].normal_index != 0 && triangle.vertices[2].normal_index != 0)
										 ? stl_detail::normalize_normal(normals[normalize_index(triangle.vertices[0].normal_index, normals.size())])
										 : compute_face_normal(a, b, c);
			auto const fallback_color = triangle.has_color ? triangle.color : color;
			auto const a_color = position_colors[a_index].value_or(fallback_color);
			auto const b_color = position_colors[b_index].value_or(fallback_color);
			auto const c_color = position_colors[c_index].value_or(fallback_color);

			append_model_vertex(model, a, a_color, face_normal);
			append_model_vertex(model, b, b_color, face_normal);
			append_model_vertex(model, c, c_color, face_normal);
			stl_detail::accumulate_vertex_normal(accumulated_normals, a, face_normal);
			stl_detail::accumulate_vertex_normal(accumulated_normals, b, face_normal);
			stl_detail::accumulate_vertex_normal(accumulated_normals, c, face_normal);
		}

		stl_detail::apply_smoothed_normals(model, accumulated_normals);
		return model;
	}

	std::unordered_set<stl_detail::point_position_key, stl_detail::point_position_key_hash> unique_positions;
	unique_positions.reserve(triangles.size() * 3);
	for (auto const& triangle : triangles) {
		for (auto const& face_vertex : triangle.vertices) {
			auto const& position = positions[normalize_index(face_vertex.position_index, positions.size())];
			auto const position_index = normalize_index(face_vertex.position_index, positions.size());
			auto normal = std::array<float, 3>{0.0f, 0.0f, 1.0f};
			if (face_vertex.normal_index != 0) {
				normal = normals[normalize_index(face_vertex.normal_index, normals.size())];
			}
			auto const fallback_color = triangle.has_color ? triangle.color : color;
			auto const vertex_color = position_colors[position_index].value_or(fallback_color);
			stl_detail::append_unique_point(model, unique_positions, position, vertex_color, normal);
		}
	}

	return model;
}

auto load_obj_model(std::filesystem::path const& path, model_representation representation, std::array<float, 3> const& color) -> model_data {
	std::ifstream stream(path);
	if (!stream.is_open()) {
		throw std::runtime_error("Failed to open OBJ file: " + path.string());
	}

	std::vector<std::array<float, 3>> positions;
	std::vector<std::optional<std::array<float, 3>>> position_colors;
	std::vector<std::array<float, 3>> normals;
	std::vector<obj_triangle> triangles;
	std::unordered_map<std::string, obj_material> materials;
	std::string current_material_name;
	std::string line;

	while (std::getline(stream, line)) {
		auto const trimmed = trim_ascii(line);
		if (trimmed.empty() || trimmed[0] == '#') {
			continue;
		}

		std::istringstream line_stream(trimmed);
		std::string prefix;
		line_stream >> prefix;

		if (prefix == "v") {
			std::array<float, 3> position{};
			line_stream >> position[0] >> position[1] >> position[2];
			positions.push_back(position);
			std::array<float, 3> vertex_color{};
			if (line_stream >> vertex_color[0] >> vertex_color[1] >> vertex_color[2]) {
				position_colors.push_back(normalize_color(vertex_color));
			} else {
				position_colors.push_back(std::nullopt);
			}
		} else if (prefix == "vn") {
			std::array<float, 3> normal{};
			line_stream >> normal[0] >> normal[1] >> normal[2];
			normals.push_back(stl_detail::normalize_normal(normal));
		} else if (prefix == "mtllib") {
			std::string material_library_name;
			std::getline(line_stream >> std::ws, material_library_name);
			auto const material_entries = parse_obj_material_library(path, material_library_name);
			materials.insert(material_entries.begin(), material_entries.end());
		} else if (prefix == "usemtl") {
			line_stream >> current_material_name;
		} else if (prefix == "f") {
			std::vector<obj_face_vertex> face_vertices;
			std::string vertex_token;
			while (line_stream >> vertex_token) {
				face_vertices.push_back(parse_obj_face_vertex(vertex_token));
			}

			if (face_vertices.size() < 3) {
				continue;
			}

			for (size_t index = 1; index + 1 < face_vertices.size(); ++index) {
				auto triangle = obj_triangle{.vertices = {face_vertices[0], face_vertices[index], face_vertices[index + 1]}};
				auto const material_iterator = materials.find(current_material_name);
				if (material_iterator != materials.end() && material_iterator->second.has_diffuse_color) {
					triangle.color = material_iterator->second.diffuse_color;
					triangle.has_color = true;
				}
				triangles.push_back(triangle);
			}
		}
	}

	if (positions.empty() || triangles.empty()) {
		throw std::runtime_error("OBJ file does not contain triangle data: " + path.string());
	}

	return build_model_from_triangles(positions, position_colors, normals, triangles, representation, color);
}

struct ply_header {
	bool ascii = false;
	size_t vertex_count = 0;
	size_t face_count = 0;
	bool has_normals = false;
	bool has_faces = false;
	std::vector<std::string> vertex_properties;
};

auto parse_ply_header(std::istream& stream, std::filesystem::path const& path) -> ply_header {
	ply_header header;
	std::string line;
	bool in_vertex_element = false;

	if (!std::getline(stream, line) || trim_ascii(line) != "ply") {
		throw std::runtime_error("Invalid PLY header: " + path.string());
	}

	while (std::getline(stream, line)) {
		auto const trimmed = trim_ascii(line);
		if (trimmed == "end_header") {
			return header;
		}

		std::istringstream line_stream(trimmed);
		std::string prefix;
		line_stream >> prefix;
		if (prefix == "format") {
			std::string format_type;
			line_stream >> format_type;
			header.ascii = format_type == "ascii";
		} else if (prefix == "element") {
			std::string element_name;
			size_t count = 0;
			line_stream >> element_name >> count;
			in_vertex_element = element_name == "vertex";
			if (element_name == "vertex") {
				header.vertex_count = count;
			} else if (element_name == "face") {
				header.face_count = count;
				header.has_faces = count > 0;
			}
		} else if (prefix == "property" && in_vertex_element) {
			std::string property_type;
			std::string property_name;
			line_stream >> property_type >> property_name;
			header.vertex_properties.push_back(property_name);
			header.has_normals = header.has_normals || property_name == "nx" || property_name == "ny" || property_name == "nz";
		}
	}

	throw std::runtime_error("PLY header is incomplete: " + path.string());
}

auto property_index(std::vector<std::string> const& properties, std::string_view name) -> std::optional<size_t> {
	for (size_t index{}; index < properties.size(); ++index) {
		if (properties[index] == name) {
			return index;
		}
	}

	return std::nullopt;
}

auto load_ply_model(std::filesystem::path const& path, model_representation representation, std::array<float, 3> const& color) -> model_data {
	std::ifstream stream(path);
	if (!stream.is_open()) {
		throw std::runtime_error("Failed to open PLY file: " + path.string());
	}

	auto const header = parse_ply_header(stream, path);
	if (!header.ascii) {
		throw std::runtime_error("Only ASCII PLY is supported: " + path.string());
	}

	auto const x_index = property_index(header.vertex_properties, "x");
	auto const y_index = property_index(header.vertex_properties, "y");
	auto const z_index = property_index(header.vertex_properties, "z");
	if (!x_index || !y_index || !z_index) {
		throw std::runtime_error("PLY vertex positions are missing: " + path.string());
	}

	auto const nx_index = property_index(header.vertex_properties, "nx");
	auto const ny_index = property_index(header.vertex_properties, "ny");
	auto const nz_index = property_index(header.vertex_properties, "nz");
	auto red_index = property_index(header.vertex_properties, "red");
	if (!red_index) {
		red_index = property_index(header.vertex_properties, "r");
	}
	auto green_index = property_index(header.vertex_properties, "green");
	if (!green_index) {
		green_index = property_index(header.vertex_properties, "g");
	}
	auto blue_index = property_index(header.vertex_properties, "blue");
	if (!blue_index) {
		blue_index = property_index(header.vertex_properties, "b");
	}

	std::vector<std::array<float, 3>> positions;
	positions.reserve(header.vertex_count);
	std::vector<std::array<float, 3>> normals;
	normals.reserve(header.vertex_count);
	std::vector<std::optional<std::array<float, 3>>> colors;
	colors.reserve(header.vertex_count);

	std::string line;
	for (size_t vertex_index{}; vertex_index < header.vertex_count; ++vertex_index) {
		if (!std::getline(stream, line)) {
			throw std::runtime_error("PLY vertex data is incomplete: " + path.string());
		}

		std::istringstream line_stream(line);
		std::vector<float> values(header.vertex_properties.size(), 0.0f);
		for (size_t property = 0; property < header.vertex_properties.size(); ++property) {
			line_stream >> values[property];
		}

		positions.push_back({values[*x_index], values[*y_index], values[*z_index]});
		if (nx_index && ny_index && nz_index) {
			normals.push_back(stl_detail::normalize_normal({values[*nx_index], values[*ny_index], values[*nz_index]}));
		} else {
			normals.push_back({0.0f, 0.0f, 1.0f});
		}
		if (red_index && green_index && blue_index) {
			colors.push_back(normalize_color({values[*red_index], values[*green_index], values[*blue_index]}));
		} else {
			colors.push_back(std::nullopt);
		}
	}

	if (representation == model_representation::point_cloud || !header.has_faces) {
		auto model = model_data{.bounds = stl_detail::default_bounds()};
		std::unordered_set<stl_detail::point_position_key, stl_detail::point_position_key_hash> unique_positions;
		unique_positions.reserve(positions.size());
		for (size_t vertex_index{}; vertex_index < positions.size(); ++vertex_index) {
			stl_detail::append_unique_point(model, unique_positions, positions[vertex_index], colors[vertex_index].value_or(color), normals[vertex_index]);
		}
		return model;
	}

	std::vector<obj_triangle> triangles;
	triangles.reserve(header.face_count);
	for (size_t face_index{}; face_index < header.face_count; ++face_index) {
		if (!std::getline(stream, line)) {
			throw std::runtime_error("PLY face data is incomplete: " + path.string());
		}

		std::istringstream line_stream(line);
		size_t vertex_count = 0;
		line_stream >> vertex_count;
		if (vertex_count < 3) {
			continue;
		}

		std::vector<int32_t> indices(vertex_count, 0);
		for (size_t index{}; index < vertex_count; ++index) {
			line_stream >> indices[index];
		}

		for (size_t index = 1; index + 1 < indices.size(); ++index) {
			triangles.push_back(obj_triangle{
				.vertices = {
					obj_face_vertex{.position_index = indices[0] + 1, .normal_index = 0},
					obj_face_vertex{.position_index = indices[index] + 1, .normal_index = 0},
					obj_face_vertex{.position_index = indices[index + 1] + 1, .normal_index = 0}}});
		}
	}

	return build_model_from_triangles(positions, colors, normals, triangles, representation, color);
}
} // namespace loader_detail

auto load_model(
	std::filesystem::path const& path,
	model_representation representation,
	point const& color) -> model_data {
	if (!std::filesystem::exists(path)) {
		throw std::runtime_error("Model file does not exist: " + path.string());
	}

	auto extension = path.extension().string();
	std::ranges::transform(extension, extension.begin(), [](unsigned char character) {
		return static_cast<char>(std::tolower(character));
	});

	if (extension == ".stl") {
		return representation == model_representation::point_cloud
				   ? load_stl_point_cloud(path, color)
				   : load_stl_mesh(path, color);
	}
	if (extension == ".obj") {
		return loader_detail::load_obj_model(path, representation, color);
	}
	if (extension == ".ply") {
		return loader_detail::load_ply_model(path, representation, color);
	}

	throw std::runtime_error("Unsupported model file format: " + path.string());
}
} // namespace modern_vulkan

namespace modern_vulkan
{
namespace renderer_detail
{
struct compiled_shader_paths {
	std::filesystem::path vertex_path;
	std::filesystem::path fragment_path;
};

struct shader_push_constants {
	std::array<float, 16> mvp;
	std::array<float, 4> lighting_info;
};

auto quote_path(std::filesystem::path const& path) -> std::wstring {
	return L'"' + path.wstring() + L'"';
}

auto glslang_validator_path() -> std::filesystem::path {
	char *vulkan_sdk = nullptr;
	size_t vulkan_sdk_length = 0;
	if (_dupenv_s(&vulkan_sdk, &vulkan_sdk_length, "VULKAN_SDK") != 0 || vulkan_sdk == nullptr) {
		throw std::runtime_error("VULKAN_SDK is not set. glslangValidator is required to compile shaders.");
	}
	std::unique_ptr<char, decltype(&std::free)> vulkan_sdk_guard(vulkan_sdk, &std::free);

	auto const sdk_path = std::filesystem::path(vulkan_sdk);
	auto const windows_path = sdk_path / "Bin" / "glslangValidator.exe";
	if (std::filesystem::exists(windows_path)) {
		return windows_path;
	}

	auto const cross_platform_path = sdk_path / "Bin" / "glslangValidator";
	if (std::filesystem::exists(cross_platform_path)) {
		return cross_platform_path;
	}

	throw std::runtime_error("glslangValidator was not found in the Vulkan SDK installation.");
}

void write_text_file(std::filesystem::path const& path, std::string_view content) {
	std::ofstream stream(path, std::ios::binary | std::ios::trunc);
	if (!stream.is_open()) {
		throw std::runtime_error("Failed to open shader file for writing: " + path.string());
	}

	stream.write(content.data(), static_cast<std::streamsize>(content.size()));
	if (!stream) {
		throw std::runtime_error("Failed to write shader source file: " + path.string());
	}
}

void compile_shader(std::filesystem::path const& compiler, std::filesystem::path const& source, std::filesystem::path const& output) {
	std::wstring command_line = quote_path(compiler) + L" -V " + quote_path(source) + L" -o " + quote_path(output);
	STARTUPINFOW startup_info{};
	startup_info.cb = sizeof(startup_info);
	PROCESS_INFORMATION process_information{};
	std::vector<wchar_t> mutable_command_line(command_line.begin(), command_line.end());
	mutable_command_line.push_back(L'\0');

	if (!CreateProcessW(
			nullptr,
			mutable_command_line.data(),
			nullptr,
			nullptr,
			FALSE,
			0,
			nullptr,
			nullptr,
			&startup_info,
			&process_information)) {
		throw std::runtime_error("Failed to start glslangValidator for shader compilation.");
	}

	WaitForSingleObject(process_information.hProcess, INFINITE);
	DWORD exit_code = 0;
	GetExitCodeProcess(process_information.hProcess, &exit_code);
	CloseHandle(process_information.hThread);
	CloseHandle(process_information.hProcess);

	if (exit_code != 0 || !std::filesystem::exists(output)) {
		throw std::runtime_error("Failed to compile shader source: " + source.string());
	}
}

auto ensure_compiled_point_cloud_shaders() -> compiled_shader_paths {
	auto const shader_directory = std::filesystem::temp_directory_path() / "modern_vulkan_point_cloud_shaders";
	std::filesystem::create_directories(shader_directory);

	auto const vertex_source = shader_directory / "point_cloud_runtime.vert";
	auto const fragment_source = shader_directory / "point_cloud_runtime.frag";
	auto const vertex_spv = shader_directory / "point_cloud_runtime.vert.spv";
	auto const fragment_spv = shader_directory / "point_cloud_runtime.frag.spv";

	constexpr std::string_view vertex_shader_source = R"(#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;

layout(location = 0) out vec3 outColor;
layout(location = 1) out vec3 outNormal;

layout(push_constant) uniform PushConstants {
	mat4 mvp;
    vec4 lightingInfo;
} pushConstants;

void main() {
	gl_Position = pushConstants.mvp * vec4(inPosition, 1.0);
	gl_PointSize = 4.0;
	outColor = inColor;
   outNormal = normalize(mat3(pushConstants.mvp) * inNormal);
}
)";
	constexpr std::string_view fragment_shader_source = R"(#version 450

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec3 inNormal;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
	mat4 mvp;
	vec4 lightingInfo;
} pushConstants;

void main() {
  float lightingEnabled = pushConstants.lightingInfo.w;
	vec3 lightDirection = normalize(pushConstants.lightingInfo.xyz);
	float diffuse = lightingEnabled > 0.5 ? max(dot(normalize(inNormal), lightDirection), 0.0) : 1.0;
	float ambient = lightingEnabled > 0.5 ? 0.25 : 0.0;
	outColor = vec4(inColor * clamp(ambient + diffuse, 0.0, 1.0), 1.0);
}
)";

	write_text_file(vertex_source, vertex_shader_source);
	write_text_file(fragment_source, fragment_shader_source);

	auto const compiler = glslang_validator_path();
	compile_shader(compiler, vertex_source, vertex_spv);
	compile_shader(compiler, fragment_source, fragment_spv);

	return compiled_shader_paths{
		.vertex_path = vertex_spv,
		.fragment_path = fragment_spv};
}

auto find_graphics_queue_family_index(std::span<queue_family_support const> queue_families) -> uint32_t {
	for (auto const& queue_family : queue_families) {
		if (queue_family.graphics) {
			return queue_family.index;
		}
	}

	throw std::runtime_error("No graphics queue family is available.");
}

auto find_present_queue_family_index(std::span<queue_family_support const> queue_families) -> uint32_t {
	for (auto const& queue_family : queue_families) {
		if (queue_family.present) {
			return queue_family.index;
		}
	}

	throw std::runtime_error("No present queue family is available.");
}

void create_buffer(
	VkDevice logical_device,
	VkPhysicalDevice physical_device,
	VkDeviceSize size,
	VkBufferUsageFlags usage,
	VkMemoryPropertyFlags memory_properties,
	VkBuffer& buffer,
	VkDeviceMemory& buffer_memory) {
	VkBufferCreateInfo buffer_create_info{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE};
	if (vkCreateBuffer(logical_device, &buffer_create_info, nullptr, &buffer) != VK_SUCCESS) {
		throw std::runtime_error("vkCreateBuffer failed.");
	}

	VkMemoryRequirements memory_requirements{};
	vkGetBufferMemoryRequirements(logical_device, buffer, &memory_requirements);

	VkMemoryAllocateInfo allocate_info{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = memory_requirements.size,
		.memoryTypeIndex = detail::find_memory_type(physical_device, memory_requirements.memoryTypeBits, memory_properties)};
	if (vkAllocateMemory(logical_device, &allocate_info, nullptr, &buffer_memory) != VK_SUCCESS) {
		vkDestroyBuffer(logical_device, buffer, nullptr);
		buffer = VK_NULL_HANDLE;
		throw std::runtime_error("vkAllocateMemory failed for buffer.");
	}

	if (vkBindBufferMemory(logical_device, buffer, buffer_memory, 0) != VK_SUCCESS) {
		vkFreeMemory(logical_device, buffer_memory, nullptr);
		vkDestroyBuffer(logical_device, buffer, nullptr);
		buffer_memory = VK_NULL_HANDLE;
		buffer = VK_NULL_HANDLE;
		throw std::runtime_error("vkBindBufferMemory failed.");
	}
}

void copy_buffer(
	VkDevice logical_device,
	VkCommandPool command_pool,
	VkQueue graphics_queue,
	VkBuffer source_buffer,
	VkBuffer destination_buffer,
	VkDeviceSize size) {
	VkCommandBufferAllocateInfo command_buffer_allocate_info{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = command_pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1};
	VkCommandBuffer command_buffer = VK_NULL_HANDLE;
	if (vkAllocateCommandBuffers(logical_device, &command_buffer_allocate_info, &command_buffer) != VK_SUCCESS) {
		throw std::runtime_error("vkAllocateCommandBuffers failed for transfer command buffer.");
	}

	try {
		VkCommandBufferBeginInfo begin_info{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
		if (vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS) {
			throw std::runtime_error("vkBeginCommandBuffer failed for transfer command buffer.");
		}

		VkBufferCopy copy_region{
			.srcOffset = 0,
			.dstOffset = 0,
			.size = size};
		vkCmdCopyBuffer(command_buffer, source_buffer, destination_buffer, 1, &copy_region);

		if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
			throw std::runtime_error("vkEndCommandBuffer failed for transfer command buffer.");
		}

		VkSubmitInfo submit_info{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.commandBufferCount = 1,
			.pCommandBuffers = &command_buffer};
		if (vkQueueSubmit(graphics_queue, 1, &submit_info, VK_NULL_HANDLE) != VK_SUCCESS) {
			throw std::runtime_error("vkQueueSubmit failed for transfer command buffer.");
		}

		vkQueueWaitIdle(graphics_queue);
	}
	catch (...) {
		vkFreeCommandBuffers(logical_device, command_pool, 1, &command_buffer);
		throw;
	}

	vkFreeCommandBuffers(logical_device, command_pool, 1, &command_buffer);
}

auto identity_matrix() -> std::array<float, 16> {
	return std::array<float, 16>{
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f};
}

auto multiply_matrix(std::array<float, 16> const& left, std::array<float, 16> const& right) -> std::array<float, 16> {
	auto result = std::array<float, 16>{};
	for (uint32_t column{}; column < 4; ++column) {
		for (uint32_t row{}; row < 4; ++row) {
			for (uint32_t index{}; index < 4; ++index) {
				result[column * 4 + row] += left[index * 4 + row] * right[column * 4 + index];
			}
		}
	}

	return result;
}

auto translation_matrix(float x, float y, float z) -> std::array<float, 16> {
	auto result = identity_matrix();
	result[12] = x;
	result[13] = y;
	result[14] = z;
	return result;
}

auto scale_matrix(float x, float y, float z) -> std::array<float, 16> {
	auto result = identity_matrix();
	result[0] = x;
	result[5] = y;
	result[10] = z;
	return result;
}

auto rotation_x_matrix(float radians) -> std::array<float, 16> {
	auto result = identity_matrix();
	auto const cosine = std::cos(radians);
	auto const sine = std::sin(radians);
	result[5] = cosine;
	result[6] = sine;
	result[9] = -sine;
	result[10] = cosine;
	return result;
}

auto rotation_y_matrix(float radians) -> std::array<float, 16> {
	auto result = identity_matrix();
	auto const cosine = std::cos(radians);
	auto const sine = std::sin(radians);
	result[0] = cosine;
	result[2] = -sine;
	result[8] = sine;
	result[10] = cosine;
	return result;
}

auto make_point_cloud_mvp(
	modern_vulkan::bounds const& model_bounds,
	float pitch_radians,
	float yaw_radians,
	float zoom_factor,
	float pan_x,
	float pan_y) -> std::array<float, 16> {
	auto const center_x = (model_bounds.min[0] + model_bounds.max[0]) * 0.5f;
	auto const center_y = (model_bounds.min[1] + model_bounds.max[1]) * 0.5f;
	auto const center_z = (model_bounds.min[2] + model_bounds.max[2]) * 0.5f;
	auto const extent_x = model_bounds.max[0] - model_bounds.min[0];
	auto const extent_y = model_bounds.max[1] - model_bounds.min[1];
	auto const extent_z = model_bounds.max[2] - model_bounds.min[2];
	auto const max_extent = std::max({extent_x, extent_y, extent_z, 1.0e-4f});
	auto const xy_scale = (1.6f * zoom_factor) / max_extent;
	auto const depth_scale = (0.5f * zoom_factor) / max_extent;

	auto result = translation_matrix(pan_x, pan_y, 0.5f);
	result = multiply_matrix(result, scale_matrix(xy_scale, -xy_scale, depth_scale));
	result = multiply_matrix(result, rotation_y_matrix(yaw_radians));
	result = multiply_matrix(result, rotation_x_matrix(pitch_radians));
	result = multiply_matrix(result, translation_matrix(-center_x, -center_y, -center_z));
	return result;
}
} // namespace renderer_detail

struct interactive_renderer_private {
	interactive_renderer_private(
		logical_device const& logical_device,
		swapchain const& swapchain,
		std::span<point_vertex const> vertices,
		modern_vulkan::bounds const& model_bounds,
		primitive_topology topology,
		bool lighting_enabled_default)
		: logical_device_(static_cast<VkDevice>(logical_device.handle())),
		  physical_device_(static_cast<VkPhysicalDevice>(logical_device.bound_physical_device().handle())),
		  swapchain_view_(&swapchain),
		  swapchain_(static_cast<VkSwapchainKHR>(swapchain.handle())),
		  shader_paths_(renderer_detail::ensure_compiled_point_cloud_shaders()),
		  solid_pipeline_(logical_device, swapchain, shader_paths_.vertex_path, shader_paths_.fragment_path, topology, polygon_mode::solid),
		  point_count_(static_cast<uint32_t>(vertices.size())),
		  bounds_(model_bounds),
		  topology_(topology),
		  lighting_enabled_(lighting_enabled_default),
		  wireframe_supported_(topology == primitive_topology::triangle_list && logical_device.bound_physical_device().supports_fill_mode_non_solid()) {
		if (logical_device_ == VK_NULL_HANDLE || physical_device_ == VK_NULL_HANDLE || swapchain_ == VK_NULL_HANDLE) {
			throw std::runtime_error("Point cloud renderer requires valid Vulkan device and swapchain handles.");
		}
		if (point_count_ == 0) {
			throw std::runtime_error("Point cloud renderer requires at least one point.");
		}

		auto const queue_families = logical_device.queue_families();
		graphics_queue_family_index_ = renderer_detail::find_graphics_queue_family_index(queue_families);
		present_queue_family_index_ = renderer_detail::find_present_queue_family_index(queue_families);
		vkGetDeviceQueue(logical_device_, graphics_queue_family_index_, 0, &graphics_queue_);
		vkGetDeviceQueue(logical_device_, present_queue_family_index_, 0, &present_queue_);

		VkCommandPoolCreateInfo command_pool_create_info{
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = graphics_queue_family_index_};
		if (vkCreateCommandPool(logical_device_, &command_pool_create_info, nullptr, &command_pool_) != VK_SUCCESS) {
			throw std::runtime_error("vkCreateCommandPool failed for point cloud renderer.");
		}

		update_mvp();
		if (wireframe_supported_) {
			wireframe_pipeline_ = std::make_unique<point_cloud_pipeline>(
				logical_device,
				swapchain,
				shader_paths_.vertex_path,
				shader_paths_.fragment_path,
				topology,
				polygon_mode::line);
		}
		create_vertex_buffer(vertices);
		allocate_command_buffers(swapchain);
		create_sync_objects();
	}

	~interactive_renderer_private() {
		if (logical_device_ != VK_NULL_HANDLE) {
			vkDeviceWaitIdle(logical_device_);
		}

		if (logical_device_ != VK_NULL_HANDLE && in_flight_fence_ != VK_NULL_HANDLE) {
			vkDestroyFence(logical_device_, in_flight_fence_, nullptr);
		}
		for (auto const render_finished_semaphore : render_finished_semaphores_) {
			if (logical_device_ != VK_NULL_HANDLE && render_finished_semaphore != VK_NULL_HANDLE) {
				vkDestroySemaphore(logical_device_, render_finished_semaphore, nullptr);
			}
		}
		if (logical_device_ != VK_NULL_HANDLE && image_available_semaphore_ != VK_NULL_HANDLE) {
			vkDestroySemaphore(logical_device_, image_available_semaphore_, nullptr);
		}
		if (logical_device_ != VK_NULL_HANDLE && command_pool_ != VK_NULL_HANDLE) {
			vkDestroyCommandPool(logical_device_, command_pool_, nullptr);
		}
		if (logical_device_ != VK_NULL_HANDLE && vertex_buffer_ != VK_NULL_HANDLE) {
			vkDestroyBuffer(logical_device_, vertex_buffer_, nullptr);
		}
		if (logical_device_ != VK_NULL_HANDLE && vertex_buffer_memory_ != VK_NULL_HANDLE) {
			vkFreeMemory(logical_device_, vertex_buffer_memory_, nullptr);
		}
	}

	void create_vertex_buffer(std::span<point_vertex const> vertices) {
		auto const buffer_size = static_cast<VkDeviceSize>(vertices.size() * sizeof(point_vertex));

		VkBuffer staging_buffer = VK_NULL_HANDLE;
		VkDeviceMemory staging_buffer_memory = VK_NULL_HANDLE;
		renderer_detail::create_buffer(
			logical_device_,
			physical_device_,
			buffer_size,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			staging_buffer,
			staging_buffer_memory);

		void *mapped_memory = nullptr;
		if (vkMapMemory(logical_device_, staging_buffer_memory, 0, buffer_size, 0, &mapped_memory) != VK_SUCCESS) {
			vkFreeMemory(logical_device_, staging_buffer_memory, nullptr);
			vkDestroyBuffer(logical_device_, staging_buffer, nullptr);
			throw std::runtime_error("vkMapMemory failed for point cloud staging buffer.");
		}

		std::memcpy(mapped_memory, vertices.data(), static_cast<size_t>(buffer_size));
		vkUnmapMemory(logical_device_, staging_buffer_memory);

		renderer_detail::create_buffer(
			logical_device_,
			physical_device_,
			buffer_size,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			vertex_buffer_,
			vertex_buffer_memory_);

		renderer_detail::copy_buffer(
			logical_device_,
			command_pool_,
			graphics_queue_,
			staging_buffer,
			vertex_buffer_,
			buffer_size);

		vkFreeMemory(logical_device_, staging_buffer_memory, nullptr);
		vkDestroyBuffer(logical_device_, staging_buffer, nullptr);
	}

	void allocate_command_buffers(swapchain const& swapchain) {
		auto const framebuffer_count = swapchain.framebuffer_count();
		if (framebuffer_count == 0) {
			throw std::runtime_error("Swapchain must expose at least one framebuffer.");
		}

		command_buffers_.resize(framebuffer_count);
		VkCommandBufferAllocateInfo allocate_info{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = command_pool_,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = framebuffer_count};
		if (vkAllocateCommandBuffers(logical_device_, &allocate_info, command_buffers_.data()) != VK_SUCCESS) {
			throw std::runtime_error("vkAllocateCommandBuffers failed for draw command buffers.");
		}
	}

	void record_command_buffer(uint32_t image_index) {
		if (swapchain_view_ == nullptr || image_index >= command_buffers_.size()) {
			throw std::runtime_error("Invalid swapchain image index for point cloud command buffer recording.");
		}

		auto const render_pass = static_cast<VkRenderPass>(swapchain_view_->render_pass());
		auto const extent = swapchain_view_->extent();
		auto const& active_pipeline = wireframe_enabled_ && wireframe_pipeline_ ? *wireframe_pipeline_ : solid_pipeline_;
		auto const pipeline = static_cast<VkPipeline>(active_pipeline.handle());
		auto const pipeline_layout = static_cast<VkPipelineLayout>(active_pipeline.layout());

		VkCommandBufferBeginInfo begin_info{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
		if (vkBeginCommandBuffer(command_buffers_[image_index], &begin_info) != VK_SUCCESS) {
			throw std::runtime_error("vkBeginCommandBuffer failed for draw command buffer.");
		}

		std::array<VkClearValue, 2> clear_values{
			VkClearValue{.color = {{0.05f, 0.05f, 0.08f, 1.0f}}},
			VkClearValue{.depthStencil = {.depth = 1.0f, .stencil = 0}}};
		VkRenderPassBeginInfo render_pass_begin_info{
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.renderPass = render_pass,
			.framebuffer = static_cast<VkFramebuffer>(swapchain_view_->framebuffer(image_index)),
			.renderArea = {
				.offset = {.x = 0, .y = 0},
				.extent = {
					.width = static_cast<uint32_t>(extent.w),
					.height = static_cast<uint32_t>(extent.h)}},
			.clearValueCount = static_cast<uint32_t>(clear_values.size()),
			.pClearValues = clear_values.data()};

		vkCmdBeginRenderPass(command_buffers_[image_index], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(command_buffers_[image_index], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

		VkBuffer vertex_buffers[] = {vertex_buffer_};
		VkDeviceSize vertex_offsets[] = {0};
		vkCmdBindVertexBuffers(command_buffers_[image_index], 0, 1, vertex_buffers, vertex_offsets);
		renderer_detail::shader_push_constants push_constants{
			.mvp = mvp_,
			.lighting_info = {0.35f, 0.5f, 0.8f, lighting_enabled_ && !wireframe_enabled_ ? 1.0f : 0.0f}};
		vkCmdPushConstants(
			command_buffers_[image_index],
			pipeline_layout,
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			0,
			static_cast<uint32_t>(sizeof(push_constants)),
			&push_constants);
		vkCmdDraw(command_buffers_[image_index], point_count_, 1, 0, 0);
		vkCmdEndRenderPass(command_buffers_[image_index]);

		if (vkEndCommandBuffer(command_buffers_[image_index]) != VK_SUCCESS) {
			throw std::runtime_error("vkEndCommandBuffer failed for draw command buffer.");
		}
	}

	void update_mvp() {
		mvp_ = renderer_detail::make_point_cloud_mvp(bounds_, pitch_radians_, yaw_radians_, zoom_factor_, pan_x_, pan_y_);
	}

	void begin_rotate_drag(float x, float y) {
		is_rotating_ = true;
		last_cursor_x_ = x;
		last_cursor_y_ = y;
	}

	void begin_pan_drag(float x, float y) {
		is_panning_ = true;
		last_cursor_x_ = x;
		last_cursor_y_ = y;
	}

	void update_rotate_drag(float x, float y) {
		last_pointer_x_ = x;
		last_pointer_y_ = y;
		if (!is_rotating_) {
			return;
		}

		auto const delta_x = x - last_cursor_x_;
		auto const delta_y = y - last_cursor_y_;
		last_cursor_x_ = x;
		last_cursor_y_ = y;
		yaw_radians_ -= delta_x * 0.006f;
		pitch_radians_ = std::clamp(pitch_radians_ + delta_y * 0.006f, -1.5f, 1.5f);
		update_mvp();
	}

	void end_rotate_drag() {
		is_rotating_ = false;
	}

	void update_pan_drag(float x, float y) {
		last_pointer_x_ = x;
		last_pointer_y_ = y;
		if (!is_panning_ || swapchain_view_ == nullptr) {
			return;
		}

		auto const extent = swapchain_view_->extent();
		auto const safe_width = std::max(1.0f, static_cast<float>(extent.w));
		auto const safe_height = std::max(1.0f, static_cast<float>(extent.h));
		auto const delta_x = x - last_cursor_x_;
		auto const delta_y = y - last_cursor_y_;
		last_cursor_x_ = x;
		last_cursor_y_ = y;
		pan_x_ += (delta_x * 2.0f) / safe_width;
		pan_y_ += (delta_y * 2.0f) / safe_height;
		update_mvp();
	}

	void end_pan_drag() {
		is_panning_ = false;
	}

	void zoom_camera(float delta, float x, float y) {
		if (delta == 0.0f) {
			return;
		}
		last_pointer_x_ = x;
		last_pointer_y_ = y;
		if (swapchain_view_ == nullptr) {
			return;
		}

		auto const extent = swapchain_view_->extent();
		auto const safe_width = std::max(1.0f, static_cast<float>(extent.w));
		auto const safe_height = std::max(1.0f, static_cast<float>(extent.h));
		auto const clip_x = ((x / safe_width) * 2.0f) - 1.0f;
		auto const clip_y = 1.0f - ((y / safe_height) * 2.0f);
		auto const previous_zoom_factor = zoom_factor_;
		auto const new_zoom_factor = std::clamp(zoom_factor_ * std::pow(1.15f, delta), 0.1f, 20.0f);
		auto const zoom_ratio = new_zoom_factor / previous_zoom_factor;
		pan_x_ = clip_x - ((clip_x - pan_x_) * zoom_ratio);
		pan_y_ = clip_y - ((clip_y - pan_y_) * zoom_ratio);
		zoom_factor_ = new_zoom_factor;
		update_mvp();
	}

	void toggle_wireframe() {
		if (!wireframe_supported_) {
			return;
		}

		wireframe_enabled_ = !wireframe_enabled_;
	}

	void toggle_lighting() {
		if (topology_ != primitive_topology::triangle_list) {
			return;
		}

		lighting_enabled_ = !lighting_enabled_;
	}

	auto wireframe_enabled() const -> bool {
		return wireframe_enabled_;
	}

	auto lighting_enabled() const -> bool {
		return lighting_enabled_;
	}

	void create_sync_objects() {
		VkSemaphoreCreateInfo semaphore_create_info{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
		if (vkCreateSemaphore(logical_device_, &semaphore_create_info, nullptr, &image_available_semaphore_) != VK_SUCCESS) {
			throw std::runtime_error("vkCreateSemaphore failed for point cloud renderer.");
		}

		render_finished_semaphores_.resize(command_buffers_.size(), VK_NULL_HANDLE);
		for (auto& render_finished_semaphore : render_finished_semaphores_) {
			if (vkCreateSemaphore(logical_device_, &semaphore_create_info, nullptr, &render_finished_semaphore) != VK_SUCCESS) {
				throw std::runtime_error("vkCreateSemaphore failed for point cloud renderer.");
			}
		}

		VkFenceCreateInfo fence_create_info{
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.flags = VK_FENCE_CREATE_SIGNALED_BIT};
		if (vkCreateFence(logical_device_, &fence_create_info, nullptr, &in_flight_fence_) != VK_SUCCESS) {
			throw std::runtime_error("vkCreateFence failed for point cloud renderer.");
		}
	}

	void draw_frame() {
		if (vkWaitForFences(logical_device_, 1, &in_flight_fence_, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
			throw std::runtime_error("vkWaitForFences failed for point cloud renderer.");
		}

		uint32_t image_index = 0;
		auto const acquire_result = vkAcquireNextImageKHR(
			logical_device_,
			swapchain_,
			UINT64_MAX,
			image_available_semaphore_,
			VK_NULL_HANDLE,
			&image_index);
		if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
			return;
		}
		if (acquire_result != VK_SUCCESS && acquire_result != VK_SUBOPTIMAL_KHR) {
			throw std::runtime_error("vkAcquireNextImageKHR failed for point cloud renderer.");
		}

		if (vkResetFences(logical_device_, 1, &in_flight_fence_) != VK_SUCCESS) {
			throw std::runtime_error("vkResetFences failed for point cloud renderer.");
		}
		if (vkResetCommandBuffer(command_buffers_[image_index], 0) != VK_SUCCESS) {
			throw std::runtime_error("vkResetCommandBuffer failed for point cloud renderer.");
		}
		record_command_buffer(image_index);

		VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VkSubmitInfo submit_info{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &image_available_semaphore_,
			.pWaitDstStageMask = &wait_stage,
			.commandBufferCount = 1,
			.pCommandBuffers = &command_buffers_[image_index],
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = &render_finished_semaphores_[image_index]};
		if (vkQueueSubmit(graphics_queue_, 1, &submit_info, in_flight_fence_) != VK_SUCCESS) {
			throw std::runtime_error("vkQueueSubmit failed for point cloud renderer.");
		}

		VkPresentInfoKHR present_info{
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &render_finished_semaphores_[image_index],
			.swapchainCount = 1,
			.pSwapchains = &swapchain_,
			.pImageIndices = &image_index};
		auto const present_result = vkQueuePresentKHR(present_queue_, &present_info);
		if (present_result != VK_SUCCESS && present_result != VK_SUBOPTIMAL_KHR && present_result != VK_ERROR_OUT_OF_DATE_KHR) {
			throw std::runtime_error("vkQueuePresentKHR failed for point cloud renderer.");
		}
	}

	VkDevice logical_device_ = VK_NULL_HANDLE;
	VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
	swapchain const *swapchain_view_ = nullptr;
	VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
	uint32_t graphics_queue_family_index_ = 0;
	uint32_t present_queue_family_index_ = 0;
	VkQueue graphics_queue_ = VK_NULL_HANDLE;
	VkQueue present_queue_ = VK_NULL_HANDLE;
	VkCommandPool command_pool_ = VK_NULL_HANDLE;
	VkBuffer vertex_buffer_ = VK_NULL_HANDLE;
	VkDeviceMemory vertex_buffer_memory_ = VK_NULL_HANDLE;
	std::vector<VkCommandBuffer> command_buffers_;
	renderer_detail::compiled_shader_paths shader_paths_;
	point_cloud_pipeline solid_pipeline_;
	std::unique_ptr<point_cloud_pipeline> wireframe_pipeline_;
	VkSemaphore image_available_semaphore_ = VK_NULL_HANDLE;
	std::vector<VkSemaphore> render_finished_semaphores_;
	VkFence in_flight_fence_ = VK_NULL_HANDLE;
	uint32_t point_count_ = 0;
	bounds bounds_{};
	primitive_topology topology_ = primitive_topology::point_list;
	bool lighting_enabled_ = false;
	bool wireframe_supported_ = false;
	bool wireframe_enabled_ = false;
	float pitch_radians_ = 0.0f;
	float yaw_radians_ = 0.0f;
	float zoom_factor_ = 1.0f;
	float pan_x_ = 0.0f;
	float pan_y_ = 0.0f;
	bool is_rotating_ = false;
	bool is_panning_ = false;
	float last_cursor_x_ = 0.0f;
	float last_cursor_y_ = 0.0f;
	float last_pointer_x_ = 0.0f;
	float last_pointer_y_ = 0.0f;
	std::array<float, 16> mvp_{};
};

struct point_cloud_renderer_private : interactive_renderer_private {
	point_cloud_renderer_private(
		logical_device const& logical_device,
		swapchain const& swapchain,
		model_data const& point_cloud)
		: interactive_renderer_private(
			  logical_device,
			  swapchain,
			  std::span<point_vertex const>(point_cloud.vertices.data(), point_cloud.vertices.size()),
			  point_cloud.bounds,
			  primitive_topology::point_list,
			  false) {
	}
};

struct mesh_renderer_private : interactive_renderer_private {
	mesh_renderer_private(
		logical_device const& logical_device,
		swapchain const& swapchain,
		model_data const& mesh)
		: interactive_renderer_private(
			  logical_device,
			  swapchain,
			  std::span<point_vertex const>(mesh.vertices.data(), mesh.vertices.size()),
			  mesh.bounds,
			  primitive_topology::triangle_list,
			  true) {
	}
};

point_cloud_renderer::point_cloud_renderer(
	logical_device const& logical_device,
	swapchain const& swapchain,
	model_data const& point_cloud)
	: d(new point_cloud_renderer_private(logical_device, swapchain, point_cloud)) {
}

point_cloud_renderer::~point_cloud_renderer() {
	delete d;
}

point_cloud_renderer::point_cloud_renderer(point_cloud_renderer&& other) noexcept : d(other.d) {
	other.d = nullptr;
}

point_cloud_renderer& point_cloud_renderer::operator=(point_cloud_renderer&& other) noexcept {
	if (this != &other) {
		delete std::exchange(d, std::exchange(other.d, nullptr));
	}

	return *this;
}

void point_cloud_renderer::begin_rotate_drag(float x, float y) {
	if (d == nullptr) {
		return;
	}

	d->begin_rotate_drag(x, y);
}

void point_cloud_renderer::begin_pan_drag(float x, float y) {
	if (d == nullptr) {
		return;
	}

	d->begin_pan_drag(x, y);
}

void point_cloud_renderer::update_rotate_drag(float x, float y) {
	if (d == nullptr) {
		return;
	}

	d->update_rotate_drag(x, y);
}

void point_cloud_renderer::end_rotate_drag() {
	if (d == nullptr) {
		return;
	}

	d->end_rotate_drag();
}

void point_cloud_renderer::update_pan_drag(float x, float y) {
	if (d == nullptr) {
		return;
	}

	d->update_pan_drag(x, y);
}

void point_cloud_renderer::end_pan_drag() {
	if (d == nullptr) {
		return;
	}

	d->end_pan_drag();
}

void point_cloud_renderer::zoom_camera(float delta, float x, float y) {
	if (d == nullptr) {
		return;
	}

	d->zoom_camera(delta, x, y);
}

void point_cloud_renderer::draw_frame() const {
	if (d == nullptr) {
		return;
	}

	d->draw_frame();
}

mesh_renderer::mesh_renderer(
	logical_device const& logical_device,
	swapchain const& swapchain,
	model_data const& mesh)
	: d(new mesh_renderer_private(logical_device, swapchain, mesh)) {
}

mesh_renderer::~mesh_renderer() {
	delete d;
}

mesh_renderer::mesh_renderer(mesh_renderer&& other) noexcept : d(other.d) {
	other.d = nullptr;
}

mesh_renderer& mesh_renderer::operator=(mesh_renderer&& other) noexcept {
	if (this != &other) {
		delete std::exchange(d, std::exchange(other.d, nullptr));
	}

	return *this;
}

void mesh_renderer::begin_rotate_drag(float x, float y) {
	if (d == nullptr) {
		return;
	}

	d->begin_rotate_drag(x, y);
}

void mesh_renderer::begin_pan_drag(float x, float y) {
	if (d == nullptr) {
		return;
	}

	d->begin_pan_drag(x, y);
}

void mesh_renderer::update_rotate_drag(float x, float y) {
	if (d == nullptr) {
		return;
	}

	d->update_rotate_drag(x, y);
}

void mesh_renderer::end_rotate_drag() {
	if (d == nullptr) {
		return;
	}

	d->end_rotate_drag();
}

void mesh_renderer::update_pan_drag(float x, float y) {
	if (d == nullptr) {
		return;
	}

	d->update_pan_drag(x, y);
}

void mesh_renderer::end_pan_drag() {
	if (d == nullptr) {
		return;
	}

	d->end_pan_drag();
}

void mesh_renderer::zoom_camera(float delta, float x, float y) {
	if (d == nullptr) {
		return;
	}

	d->zoom_camera(delta, x, y);
}

void mesh_renderer::toggle_wireframe() {
	if (d == nullptr) {
		return;
	}

	d->toggle_wireframe();
}

void mesh_renderer::toggle_lighting() {
	if (d == nullptr) {
		return;
	}

	d->toggle_lighting();
}

auto mesh_renderer::wireframe_enabled() const -> bool {
	if (d == nullptr) {
		return false;
	}

	return d->wireframe_enabled();
}

auto mesh_renderer::lighting_enabled() const -> bool {
	if (d == nullptr) {
		return false;
	}

	return d->lighting_enabled();
}

void mesh_renderer::draw_frame() const {
	if (d == nullptr) {
		return;
	}

	d->draw_frame();
}
} // namespace modern_vulkan
