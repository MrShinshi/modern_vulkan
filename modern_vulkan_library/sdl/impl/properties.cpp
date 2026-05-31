#include <sdl/properties.h>
#include <sdl/impl/properties_private.h>
#include <common/modern_vulkan_exception.h>
#include <stdexcept>
#include <utility>

namespace
{
void set_string_property(
	SDL_PropertiesID props,
	char const *name,
	char const *value) {
	if (!SDL_SetStringProperty(props, name, value)) {
		throw modern_vulkan::sdl::exception("SDL_SetStringProperty failed");
	}
}

void set_number_property(
	SDL_PropertiesID props,
	char const *name,
	Sint64 value) {
	if (!SDL_SetNumberProperty(props, name, value)) {
		throw modern_vulkan::sdl::exception("SDL_SetNumberProperty failed");
	}
}

void set_boolean_property(
	SDL_PropertiesID props,
	char const *name,
	bool value) {
	if (!SDL_SetBooleanProperty(props, name, value)) {
		throw modern_vulkan::sdl::exception("SDL_SetBooleanProperty failed");
	}
}
} // namespace

namespace modern_vulkan::sdl
{
struct window_properties_private {
	SDL_PropertiesID props;

	window_properties_private() : props(SDL_CreateProperties()) {
	}

	~window_properties_private() {
		SDL_DestroyProperties(props);
	}
};
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
auto window_properties::handle() const -> uint32_t {
	return d->props;
}
void window_properties::title(std::string_view title) {
	set_string_property(d->props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, title.data());
}
void window_properties::rect(modern_vulkan::rect rect) {
	set_number_property(d->props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, static_cast<int>(rect.w));
	set_number_property(d->props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, static_cast<int>(rect.h));
}
void window_properties::vulkan_support(bool vulkan_support) {
	set_boolean_property(d->props, SDL_PROP_WINDOW_CREATE_VULKAN_BOOLEAN, vulkan_support);
}
void window_properties::pos(modern_vulkan::position pos) {
	set_number_property(d->props, SDL_PROP_WINDOW_CREATE_X_NUMBER, static_cast<int>(pos.x));
	set_number_property(d->props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, static_cast<int>(pos.y));
}

void window_properties::resizable(bool resizable) {
	set_boolean_property(d->props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, resizable);
}

window_properties::window_properties(window_properties&& other) noexcept : d(other.d) {
	other.d = nullptr;
}

window_properties& window_properties::operator=(window_properties&& other) noexcept {
	if (this != &other) {
		delete std::exchange(d, std::exchange(other.d, nullptr));
	}

	return *this;
}
} // namespace modern_vulkan::sdl