#include "properties.h"
#include "properties_private.h"

namespace modern_vulkan
{
window_properties::window_properties(
	std::string_view title,
	modern_vulkan::rect rect,
	bool vulkan_support)
	: d(new window_properties_private()) {
	this->title(title);
	this->rect(rect);
	this->vulkan_support(vulkan_support);
}
window_properties::~window_properties() {
	delete d;
}
void window_properties::title(std::string_view title) {
	SDL_SetStringProperty(d->props, "window.title", title.data());
}
void window_properties::rect(modern_vulkan::rect rect) {
	SDL_SetNumberProperty(d->props, "window.width", static_cast<int>(rect.w));
	SDL_SetNumberProperty(d->props, "window.height", static_cast<int>(rect.h));
}
void window_properties::vulkan_support(bool vulkan_support) {
	SDL_SetBooleanProperty(d->props, "window.vulkan", vulkan_support);
}
void window_properties::pos(modern_vulkan::pos pos) {
	SDL_SetNumberProperty(d->props, "window.x", static_cast<int>(pos.x));
	SDL_SetNumberProperty(d->props, "window.y", static_cast<int>(pos.y));
}

window_properties::window_properties(window_properties&& other) noexcept : d(other.d) {
	other.d = nullptr;
}

window_properties& window_properties::operator=(window_properties&& other) noexcept {
	delete std::exchange(d, std::exchange(other.d, nullptr));
	return *this;
}

window_properties::window_properties(window_properties const& other)
	: d(other.d ? new window_properties_private(*other.d) : nullptr) {
}

window_properties& window_properties::operator=(window_properties const& other) {
	if (this != &other) {
#pragma warning(push)
#pragma warning(disable : 6011)
		delete std::exchange(d, other.d ? new window_properties_private(*other.d) : nullptr);
#pragma warning(pop)
	}
	return *this;
}
} // namespace modern_vulkan