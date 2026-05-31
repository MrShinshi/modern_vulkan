# Copilot Instructions

## 项目指南
- In this repo, prefer binding a physical_device to its parent instance in the constructor so support-check methods do not require an instance parameter.
- In this repo, prefer using Vulkan-provided name macros for extensions/layers instead of raw string literals like VK_KHR_surface when available.
- In this repo, prefer descriptive enums over raw uint8_t values in public event models such as mouse_event.button.