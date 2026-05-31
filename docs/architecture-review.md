# modern_vulkan 架构评审

## 评审范围

本评审基于当前仓库中的实际代码结构，而不是 README 中早期版本的描述。当前项目已经从“SDL 窗口 + Vulkan 实例预留”演进为一个包含窗口系统、Vulkan 初始化链路、交换链以及基础渲染器的实验型图形项目。

## 当前实际结构

```text
modern_vulkan/
├─ modern_vulkan/                         # 示例程序 / 组合根
│  ├─ modern_vulkan.cpp
│  └─ shaders/
├─ modern_vulkan_library/
│  ├─ common/                             # 公共类型、导出宏、异常
│  ├─ sdl/                                # SDL 生命周期、窗口、输入事件
│  │  └─ impl/
│  └─ vulkan/                             # Vulkan 启动链路与渲染能力
│     └─ impl/
├─ docs/
│  └─ architecture-review.md
├─ .github/
│  └─ copilot-instructions.md
└─ README.md
```

## 分层与职责

### 1. common 层

代表文件：

- `modern_vulkan_library/common/modern_vulkan_library.h`
- `modern_vulkan_library/common/modern_vulkan_exception.*`

职责：

- 定义导出宏 `MODERN_VULKAN_LIBRARY_EXPORT`
- 定义跨模块复用的语义类型，如 `rect`、`position`、`point`
- 承载公共异常能力

评价：

- 这是当前项目最稳定的一层。
- 公共模型保持轻量，适合作为 SDL 与 Vulkan 模块的共享基础。
- `rect` / `position` / `point` 这类语义类型使公共 API 比裸坐标更清晰。

### 2. sdl 层

代表文件：

- `modern_vulkan_library/sdl/simple_direct_media_layer.h`
- `modern_vulkan_library/sdl/window.h`
- `modern_vulkan_library/sdl/properties.h`
- `modern_vulkan_library/sdl/impl/*.cpp`

职责：

- 初始化/退出 SDL 视频子系统
- 创建窗口与配置窗口属性
- 提供主循环 `execute(...)`
- 将 SDL 原始事件映射为项目自己的 `input_event`
- 查询 Vulkan 实例所需扩展

评价：

- 该层不仅负责平台窗口，还承担输入事件抽象，是当前项目的平台接入层。
- `input_event` 已经统一了鼠标、键盘、手柄事件模型，接口方向是正确的。
- `window_properties`、`window`、`simple_direct_media_layer` 都采用了资源拥有型包装，边界清晰。

需要注意：

- `simple_direct_media_layer` 同时负责事件循环和 Vulkan 扩展查询，说明 SDL 层与 Vulkan 启动链路存在现实耦合。
- 这种耦合在 Vulkan + SDL 项目里是可接受的，但后续如果要支持多平台后端或无窗口模式，建议把“Vulkan 启动所需平台能力”再抽成单独适配接口。

### 3. vulkan 层

代表文件：

- `modern_vulkan_library/vulkan/extensions.h`
- `modern_vulkan_library/vulkan/instance.h`
- `modern_vulkan_library/vulkan/physical_device.h`
- `modern_vulkan_library/vulkan/logical_device.h`
- `modern_vulkan_library/vulkan/surface.h`
- `modern_vulkan_library/vulkan/swapchain.h`
- `modern_vulkan_library/vulkan/model_loader.h`
- `modern_vulkan_library/vulkan/mesh_renderer.h`
- `modern_vulkan_library/vulkan/point_cloud_renderer.h`
- `modern_vulkan_library/vulkan/impl/*.cpp`

职责：

- 管理 Vulkan 实例、物理设备、逻辑设备、Surface、Swapchain
- 暴露扩展和队列族等显式能力查询接口
- 提供模型加载入口
- 提供点云渲染器与网格渲染器

评价：

- 这一层已经形成了从启动到渲染的最小闭环。
- 设计上没有刻意隐藏 Vulkan 概念，而是保留实例、物理设备、逻辑设备、交换链等核心对象，符合“学习型/实验型项目”的目标。
- `physical_device` 的能力查询接口比较完整，说明项目已经开始从“能跑”转向“可选择设备 / 可判断能力”的方向。

需要注意：

- `surface.h` 在公共头中直接暴露了 `SDL_Window*` 和 `vk::Instance`，这是当前公共 API 中最明显的后端泄漏点。
- 多个公共句柄接口以 `void*` 返回，ABI 简单，但类型安全较弱，也让调用者更难判断真实后端类型。
- 渲染器已经具备交互、缩放、线框/光照切换等行为，但应用层仍需要显式管理交换链重建和渲染器重建，说明图形运行时编排还停留在示例级别。

### 4. app / 示例层

代表文件：

- `modern_vulkan/modern_vulkan.cpp`

职责：

- 作为组合根装配 SDL、窗口、实例、设备、交换链、模型、渲染器
- 在主循环中处理窗口尺寸变化、FPS 标题刷新、输入与模式切换

评价：

- 目前示例程序承担了过多运行时编排职责。
- 这对实验阶段是高效的，但会让后续功能继续堆积在 `main` 中。
- 当前最适合抽离的职责是：渲染会话、输入控制器、交换链重建策略。

## 当前依赖关系

```text
common
├─> sdl
└─> vulkan

sdl ──> vulkan/extensions.h     # 查询 SDL 所需实例扩展时依赖 Vulkan 扩展枚举
vulkan ──> sdl/window.h         # 创建 Surface 时依赖 SDL 窗口
app ──> sdl + vulkan            # 示例程序组合所有模块
```

结论：

- 当前依赖关系总体可控。
- 但 `sdl` 与 `vulkan` 并不是完全解耦的平行模块，而是围绕“SDL 承载 Vulkan 窗口”这一目标形成了双向集成点。
- 这对于当前项目目标是合理的，不必为了“纯分层”强行拆开；更重要的是把集成点收敛到少数明确接口上。

## 设计优点

1. **模块边界清晰**
   - `common / sdl / vulkan / app` 四层职责比较自然。

2. **公共 API 倾向语义化**
   - 使用 `enum struct`、`rect`、`position`、`point` 等类型，优于直接暴露裸整数或魔法值。

3. **资源拥有关系明确**
   - 大部分对外对象采用 RAII 风格包装，并控制了拷贝/移动语义。

4. **实现文件集中在 `impl/`**
   - 有利于隔离实现细节，也便于后续替换底层实现。

5. **示例驱动开发路径明确**
   - 示例程序真实串起了窗口、设备、交换链和渲染器，便于持续验证架构演进。

## 主要风险与改进建议

### 1. `main` 过重

现状：

- 模式切换
- FPS 统计
- 输入映射
- 交换链尺寸检测
- 交换链重建
- 渲染器重建

都集中在 `modern_vulkan.cpp`。

建议：

- 提取 `render_session` 或 `application_controller` 之类的应用层对象。
- 让 `main` 只负责装配和启动。

### 2. 公共头仍有部分后端泄漏

现状：

- `surface.h` 直接包含 SDL/Vulkan 类型。
- 多个 API 以 `void*` 返回底层句柄。

建议：

- 对外继续保持轻量包装，但尽量把 SDL/Vulkan 重型头收回 `impl/`。
- 如果必须暴露句柄，优先使用显式别名类型而不是纯 `void*`。

### 3. 运行时重建逻辑分散

现状：

- 交换链与渲染器重建逻辑由调用者手工协调。

建议：

- 增加“交换链依赖资源组”的封装，把 swapchain、framebuffer、pipeline 相关重建收拢到一个对象。

### 4. 纯逻辑组件可测试性不足

现状：

- 当前仓库中未见测试项目。
- 很多行为仍通过示例程序验证。

建议：

- 先从不依赖窗口和 GPU 的逻辑开始补测试，例如：扩展映射、事件映射、设备筛选策略、模型加载结果校验。

## 推荐演进顺序

### 短期

1. 拆分 `modern_vulkan.cpp` 的运行时编排职责
2. 收敛交换链重建与渲染器重建逻辑
3. 更新 README，确保对外文档与代码一致

### 中期

1. 引入更明确的应用层对象
2. 收紧公共头中的 SDL/Vulkan 暴露范围
3. 建立最小测试集

### 长期

1. 抽象渲染资源生命周期
2. 完善设备选择与能力协商策略
3. 将示例程序演进为可切换场景/模型的调试壳

## 总结

当前项目的架构已经不再是“窗口 + 实例占位”的早期雏形，而是一个以 `SDL` 作为平台层、以 `Vulkan` 作为图形核心、以示例程序作为组合根的现代 C++ 图形实验项目。

整体评价：

- **方向正确**：模块边界和语义模型设计都比较清晰。
- **阶段明确**：已经完成最小渲染闭环，但应用层编排还偏手工。
- **下一步重点**：不是继续堆更多 Vulkan 类型，而是先把运行时组织结构收紧，让“可扩展性”跟上“功能增长”。
