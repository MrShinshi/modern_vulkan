#pragma once
#include <common/modern_vulkan_library.h>
#include <vulkan/stl_mesh.h>

namespace modern_vulkan
{
struct logical_device;
struct swapchain;
struct mesh_renderer_private;

struct MODERN_VULKAN_LIBRARY_EXPORT mesh_renderer {
    mesh_renderer(
        logical_device const& logical_device,
        swapchain const& swapchain,
     model_data const& mesh);
    ~mesh_renderer();

    mesh_renderer(mesh_renderer&& other) noexcept;
    mesh_renderer& operator=(mesh_renderer&& other) noexcept;

    mesh_renderer(mesh_renderer const&) = delete;
    mesh_renderer& operator=(mesh_renderer const&) = delete;

    void begin_rotate_drag(float x, float y);
    void begin_pan_drag(float x, float y);
    void update_rotate_drag(float x, float y);
    void end_rotate_drag();
    void update_pan_drag(float x, float y);
    void end_pan_drag();
    void zoom_camera(float delta, float x, float y);
    void toggle_wireframe();
    void toggle_lighting();
    auto wireframe_enabled() const -> bool;
    auto lighting_enabled() const -> bool;
    void draw_frame() const;

private:
    mesh_renderer_private *d;
};
} // namespace modern_vulkan
