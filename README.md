# modern_vulkan

一个基于 **C++20**、**SDL3** 和 **Vulkan SDK** 的现代 Vulkan 学习型/实验型项目。

当前项目分为两个部分：

- `modern_vulkan_library`：封装 SDL3 窗口创建、事件循环，以及 Vulkan 相关初始化入口
- `modern_vulkan`：控制台示例程序，用于演示库的基本使用方式

> 当前版本仍处于早期阶段。  
> 已完成 SDL3 窗口与事件循环的基础封装，并提供 Vulkan 实例扩展查询入口；真正的 Vulkan `instance` 创建逻辑仍在开发中。

---

## 功能概览

目前已实现：

- 基于 `SDL3` 的视频子系统初始化与退出
- 基于属性对象的窗口创建接口
- 简单事件循环封装
- 查询 Vulkan 实例所需扩展
- 自定义 SDL 异常类型，附带 SDL 错误信息和堆栈信息

当前未完成或仍在规划中：

- Vulkan `instance` 的完整创建
- Surface / Swapchain 封装
- Render Pass / Pipeline / Command Buffer 等渲染基础设施
- 更完整的错误处理与资源生命周期管理

---

## 项目结构

```text
modern_vulkan/
├─ modern_vulkan/                  # 示例应用
│  └─ modern_vulkan.cpp
├─ modern_vulkan_library/          # 核心库
│  ├─ modern_vulkan_library.h
│  ├─ simple_direct_media_layer.h
│  ├─ simple_direct_media_layer.cpp
│  ├─ window.h
│  ├─ window.cpp
│  ├─ properties.h
│  ├─ properties.cpp
│  ├─ instance.h
│  ├─ instance.cpp
│  ├─ vulkan_exception.h
│  └─ vulkan_exception.cpp
└─ modern_vulkan.sln
```

---

## 技术栈

- **C++20 / 最新 MSVC 标准支持**
- **SDL3**
- **Vulkan SDK**
- **Visual Studio 2026**

---

## 当前设计思路

项目希望通过较薄的一层封装，把以下职责拆分清晰：

- `simple_direct_media_layer`
  - 管理 SDL 生命周期
  - 提供事件循环
  - 查询 Vulkan 所需 SDL 扩展
- `window_properties`
  - 管理窗口创建参数
  - 使用 SDL 属性系统配置标题、大小、位置、Vulkan 支持等
- `window`
  - 持有 SDL 窗口对象
- `instance`
  - 预留 Vulkan 实例封装入口

这意味着项目目标不是隐藏 Vulkan，而是提供一个更现代、可组合的基础设施层。

---

## 环境要求

在当前工程配置下，建议使用以下环境：

- Windows 10/11
- Visual Studio 2026 或兼容 MSVC 工具链
- 已安装 Vulkan SDK
- 可用的 SDL3 库文件与头文件
- 推荐使用 **x64** 配置进行构建

> 从工程文件看，`x64` 配置中已经显式依赖：
>
> - `$(VULKAN_SDK)\Include`
> - `$(VULKAN_SDK)\Lib`
> - `vulkan-1.lib`
> - `SDL3.lib`

---

## 构建说明

### 1. 安装依赖

确保本机已安装：

- Vulkan SDK，并正确设置环境变量 `VULKAN_SDK`
- SDL3 开发库

### 2. 打开解决方案

使用 Visual Studio 打开：

- `modern_vulkan.sln`

### 3. 选择配置

推荐选择：

- **Platform**: `x64`
- **Configuration**: `Debug` 或 `Release`

### 4. 构建顺序

先构建：

- `modern_vulkan_library`

再构建并运行：

- `modern_vulkan`

---

## 示例

当前示例程序会：

1. 初始化 SDL 视频子系统
2. 创建一个窗口
3. 进入事件循环
4. 接收到退出事件后结束程序

示例代码如下：

```cpp
#include <iostream>
#include <simple_direct_media_layer.h>

int main() try {
    modern_vulkan::simple_direct_media_layer sdl_layer;
    modern_vulkan::window_properties props(
        "Hello, Modern Vulkan!",
        {1280, 720},
        false);
    props.pos({100, 100});
    auto window = sdl_layer.window(props);

    return sdl_layer.execute();
}
catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
}
```

---

## API 预览

### `modern_vulkan::simple_direct_media_layer`

负责 SDL 生命周期和主循环。

- `window(window_properties const&) -> window`
- `extensions() const -> std::vector<std::string>`
- `execute() -> int`

### `modern_vulkan::window_properties`

用于描述窗口创建参数。

支持设置：

- 标题
- 宽高
- 位置
- 是否启用 Vulkan 支持

### `modern_vulkan::window`

对 SDL 窗口对象的轻量封装。

---

## 当前状态

项目目前更适合作为：

- Vulkan 初始化流程练习项目
- SDL3 + Vulkan 集成实验项目
- 逐步演进的个人图形库雏形

而不是一个已经完整可用的渲染引擎。

---

## 后续计划

可考虑按以下路线继续迭代：

- [ ] 完成 `instance` 的 Vulkan 实例创建
- [ ] 接入 SDL 窗口 Surface 创建
- [ ] 增加物理设备选择与逻辑设备创建
- [ ] 封装 Swapchain
- [ ] 建立最小可渲染三角形示例
- [ ] 补充单元测试和示例文档
- [ ] 提供 CMake 构建支持

---

## 适合谁

这个项目适合：

- 想从零理解 Vulkan 初始化链路的开发者
- 想用现代 C++ 风格组织图形项目的人
- 希望在 SDL3 上构建 Vulkan 应用基础设施的人

---

## License

暂未声明。  
如果你准备开源，建议补充 `LICENSE` 文件，并在此处明确许可证类型。
