# Copilot Instructions

## 项目指南
- In this repo, prefer binding a physical_device to its parent instance in the constructor so support-check methods do not require an instance parameter.
- In this repo, prefer using Vulkan-provided name macros for extensions/layers instead of raw string literals like VK_KHR_surface when available.
- In this repo, prefer descriptive enums over raw uint8_t values in public event models such as mouse_event.button.

## 编码风格提炼
- In this repo, prefer keeping the current layered split: `common` for shared value types and export concerns, `sdl` for platform/input orchestration, `vulkan` for graphics bootstrap and rendering, and the app project as the composition root.
- In this repo, prefer public resource-owning wrappers to stay RAII-first and usually move-only; only allow copying when the type is intentionally a lightweight handle snapshot like `physical_device`.
- In this repo, prefer hiding heavy implementation details in `impl/` and keeping exported headers thin with forward declarations when practical.
- In this repo, prefer trailing return types, `enum struct`, and semantic value objects such as `rect`, `position`, and `point` in public APIs instead of loosely-typed primitives.
- In this repo, follow the existing formatting baseline from `.clang-format`: Microsoft-derived style, tabs for indentation/continuation, no include sorting, and avoid unrelated reformatting.
- In this repo, prefer small translation-unit helper functions in anonymous namespaces for local mapping/parsing logic.
