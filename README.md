# modern_vulkan

一个基于 **C++20**、**SDL3** 和 **Vulkan SDK** 的现代 Vulkan 学习型/实验型项目。

当前项目分为两个部分：

- `modern_vulkan_library`：封装 SDL3 窗口与输入、Vulkan 启动链路、交换链与基础渲染能力
- `modern_vulkan`：控制台示例程序，用于演示窗口创建、模型加载、交换链驱动与渲染交互

> 项目仍处于实验型迭代阶段。  
> 但当前代码已经具备从 `SDL3` 窗口、`Vulkan instance / surface / physical_device / logical_device / swapchain` 到基础点云/网格渲染的最小闭环。

---

## 架构评审

- 详细架构评审文档：[`docs/architecture-review.md`](docs/architecture-review.md)
- 本次评审结论：当前仓库已经形成 `common / sdl / vulkan / app` 的清晰分层，但示例程序仍承担较多运行时编排职责，后续应优先收敛交换链重建与渲染会话管理。

---

## 功能概览

目前已实现：

- 基于 `SDL3` 的视频子系统初始化与退出
- 基于属性对象的窗口创建接口
- 鼠标 / 键盘 / 手柄输入事件抽象
- 查询 Vulkan 实例所需扩展
- Vulkan `instance` 创建
- `surface`、物理设备、逻辑设备与 `swapchain` 初始化
- 基础模型加载
- 点云与网格两种基础渲染路径
- 运行时交互：旋转、平移、缩放、线框切换、光照切换
- 自定义 SDL 异常类型，附带 SDL 错误信息和堆栈信息

当前未完成或仍在规划中：

- 更完整的渲染资源生命周期抽象
- 交换链重建相关逻辑进一步收敛
- 更清晰的应用层/运行时编排对象
- 单元测试与更系统的示例文档
- 跨平台与构建系统进一步完善

---

## 项目结构

```text
modern_vulkan/
├─ modern_vulkan/                         # 示例程序 / 组合根
│  ├─ modern_vulkan.cpp
│  └─ shaders/
├─ modern_vulkan_library/
│  ├─ common/                             # 公共类型、导出宏、异常
│  ├─ sdl/                                # SDL 生命周期、窗口、输入事件
│  │  └─ impl/
│  └─ vulkan/                             # Vulkan 启动链路与基础渲染能力
│     └─ impl/
├─ docs/
│  └─ architecture-review.md
└─ .github/
   └─ copilot-instructions.md
```

---

## 技术栈

- **C++20 / 最新 MSVC 标准支持**
- **SDL3**
- **Vulkan SDK**
- **Visual Studio 2026**

---

## 当前设计思路

项目当前已经形成如下职责划分：

- `common`
  - 放置共享语义类型，如 `rect`、`position`、`point`
  - 放置导出宏与公共异常能力
- `sdl`
  - 管理 SDL 生命周期
  - 提供窗口、属性对象、事件循环与输入事件映射
  - 查询 Vulkan 所需 SDL 扩展
- `vulkan`
  - 管理 `instance / surface / physical_device / logical_device / swapchain`
  - 暴露设备能力、扩展、队列族等图形基础设施
  - 提供模型加载与基础渲染器
- `modern_vulkan`
  - 作为组合根装配窗口、设备、交换链、渲染器和输入响应

这意味着项目目标不是隐藏 Vulkan，而是在保留核心 Vulkan 概念的前提下，提供一个更现代、可组合的基础设施层。

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
2. 创建支持 Vulkan 的窗口
3. 创建 `instance / surface / physical_device / logical_device / swapchain`
4. 加载模型并在点云/网格两种模式间切换
5. 在事件循环中处理鼠标交互、FPS 标题刷新与交换链重建

当前完整示例位于：

- `modern_vulkan/modern_vulkan.cpp`

如果你只想快速理解架构入口，建议按下面顺序阅读：

1. `modern_vulkan/modern_vulkan.cpp`
2. `modern_vulkan_library/sdl/simple_direct_media_layer.h`
3. `modern_vulkan_library/vulkan/instance.h`
4. `modern_vulkan_library/vulkan/logical_device.h`
5. `modern_vulkan_library/vulkan/swapchain.h`

---

## API 预览

### `modern_vulkan::sdl::simple_direct_media_layer`

负责 SDL 生命周期、窗口创建、输入分发与主循环。

- `window(window_properties const&) -> window`
- `extensions() const -> std::vector<instance_extension>`
- `execute() -> int`
- `execute(std::function<void()> const&, input_callbacks const&) -> int`

### `modern_vulkan::sdl::window_properties`

用于描述窗口创建参数。

支持设置：

- 标题
- 宽高
- 位置
- 是否可调整大小
- 是否启用 Vulkan 支持

### `modern_vulkan::instance`

负责 Vulkan 实例创建以及物理设备枚举。

- `make_surface(sdl::window const&) -> surface const&`
- `physical_devices() const -> std::vector<physical_device>`

### `modern_vulkan::logical_device`

负责逻辑设备创建、队列族绑定与交换链支持查询。

### `modern_vulkan::swapchain`

负责交换链、图像视图、Render Pass 与 Framebuffer 等渲染目标资源。

### `modern_vulkan::sdl::window`

对 SDL 窗口对象的轻量封装。

---

## 当前状态

项目目前更适合作为：

- SDL3 + Vulkan 集成实验项目
- Vulkan 启动链路与交换链学习项目
- 逐步演进的个人图形基础库 / 渲染实验场

而不是一个已经完整抽象好的渲染引擎。

---

## 后续计划

可考虑按以下路线继续迭代：

- [ ] 拆分 `modern_vulkan.cpp` 中的运行时编排逻辑
- [ ] 收敛交换链与渲染器重建逻辑
- [ ] 完善设备选择与能力协商策略
- [ ] 建立更稳定的渲染资源生命周期封装
- [ ] 提供更小粒度的示例与最小测试集
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
