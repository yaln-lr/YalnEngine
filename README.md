# YalnEngine

基于 Vulkan 的轻量级渲染引擎学习项目：核心渲染库（场景树/网格/灯光/渲染器）+ Qt 桌面查看器（村庄示例场景）。

## 项目结构

```
YalnEngine/
├── CMakeLists.txt              # 根 CMake（公共依赖、configure_vulkan 函数、着色器编译）
├── core/                       # 核心渲染库
│   ├── include/                # 头文件
│   │   ├── yalnview.h          # 视图门面（编排各子系统 + 帧循环）
│   │   ├── yalninterfaces.h    # 子系统接口（IYalnScene 等）
│   │   ├── yalnscene.h         # 场景树（GPU 缓冲去重 + 场景级灯光）
│   │   ├── yalnrenderer.h      # 稳定帧驱动器（GPU 帧资源 + 管线/材质库）
│   │   ├── yalnframe.h         # 帧策略接口（IYalnFramePipeline / IYalnFramePass）
│   │   ├── yalnforwardpipeline.h # 默认 Forward 整帧策略
│   │   ├── yalnforwardpass.h   # 默认前向 Opaque Pass
│   │   ├── yalnpipelinelibrary.h # PSO 注册表（YalnPipelineHandle）
│   │   ├── yalnmateriallibrary.h # 材质注册表（YalnMaterialHandle）
│   │   ├── yalnrendertypes.h   # 句柄 / Pass 掩码 / 材质推常量
│   │   ├── yalnmesh.h          # 网格数据（YalnMesh/YalnCube/YalnSphere/YalnPyramid）
│   │   ├── yalncamera.h        # 相机基类
│   │   ├── yalnperspectivecamera.h  # 透视相机
│   │   ├── yalnnode.h          # 场景树节点（变换/开关/自旋/公告板）
│   │   ├── yalnlight.h         # 灯光系统（环境/平行/点/聚光 + GPU 打包）
│   │   └── yalnmath.h          # Eigen 四元数 ↔ glm 矩阵/向量桥接
│   └── src/                    # 实现文件
├── viewer/                     # Qt 桌面查看器（主应用，村庄示例场景）
│   ├── include/qtviewer.h      # QtViewer（QWidget + 渲染循环定时器）
│   └── src/main.cpp            # 村庄场景构建 + 菜单/相机/灯光布置
├── shader/shaders/             # 共享着色器源码（GLSL，构建时编译到 build/mingw/shaders）
│   ├── shader.vert             # 顶点着色器（GLSL）
│   └── shader.frag             # 片段着色器（Blinn-Phong 光照，GLSL）
├── docs/                       # 设计文档
│   ├── 渲染管线接口使用说明.md   # 帧策略/Pass/材质/管线扩展点
│   ├── 灯光系统设计说明.md      # 灯光架构、法线方案、衰减模型、v2 方向
│   └── 灯光使用说明.md          # 四类光源 API、调参指南、FAQ
├── thirdparty/                 # 第三方依赖（DLL、Eigen 头文件）
├── build/                      # 构建输出目录
│   └── mingw/viewer/           # yalnviewer.exe 及运行时资源
├── KEYBOARD_CONTROLS.md        # 键盘控制说明
└── README.md
```

## 环境依赖

| 依赖 | 版本 | 用途 |
|------|------|------|
| CMake | >= 3.10 | 构建系统 |
| C++ | C++17 | 语言标准 |
| Qt6 | 6.11.0 (mingw_64) | 窗口管理（YalnViewer） |
| Vulkan SDK | 1.4.341.1 | 图形 API |
| GLM | latest | 数学库（矩阵/向量运算） |
| Eigen | 5.0.1 | 四元数旋转（`thirdparty/eigen-5.0.1`，头文件库） |
| MinGW-w64 (UCRT64) | GCC 15+ | 编译器 |
| glslc | - | 着色器编译器 |

## 构建与运行

### 配置与编译

```powershell
cd f:/YalnEngine
$env:VULKAN_SDK = 'D:/VulkanSDK/1.4.341.1'
cmake -S . -B build/mingw -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=d:/QT6/6.11.0/mingw_64
cmake --build build/mingw
```

（构建时 glslc 自动把 `shader/shaders/*.vert|frag` 编译为 `build/mingw/shaders/*.spv`。）

### 运行

可执行文件：`build/mingw/viewer/yalnviewer.exe`（工作目录须为其所在目录，着色器按相对路径 `../shaders/` 加载）。运行前需保证 DLL 与校验层可寻址：

```powershell
$env:PATH       = "D:/msys64/ucrt64/bin;f:/YalnEngine/build/mingw/core;D:/QT6/6.11.0/mingw_64/bin;D:/VulkanSDK/1.4.341.1/Bin;$env:PATH"
$env:VK_LAYER_PATH = "D:/VulkanSDK/1.4.341.1/Bin"
```

### VS Code

- **编译**: `Ctrl+Shift+B`
- **调试**: `F5`（自动编译+安装后启动 GDB 调试）
- 工作目录需指向可执行文件所在目录，确保着色器相对路径正确

## 操作说明

### 场景菜单

| 操作 | 功能 |
|------|------|
| `场景(S)` → `加载村庄场景` | 构建并显示村庄示例场景（含灯光），快捷键 `Ctrl+L` |
| `场景(S)` → `应用材质/管线示例` | 需先加载场景：风车换材质乘色，1~3 号山体切换为线框 PSO |

程序启动时为空场景，需先从菜单加载；重复加载会先清空旧场景再重建（网格为 static 对象复用，GPU 缓冲按需重建）。

### 村庄示例场景（viewer）

40×40 大地图上的低多边形村庄，约 307 节点 / 259 绘制项，同类物体共享网格、GPU 缓冲去重：

| 元素 | 说明 |
|------|------|
| 15 栋房子 | 多面墙 + 门 + 四扇窗 + 屋顶（陶土红尖顶，house2 可切换平顶），沿路网/山脚/湖畔疏散分布，均不压水面 |
| 水系 | 近似圆形的湖（湖心盘 + 6 条旋转弧段叠出弧形湖岸）+ 自西向东蜿蜒入湖的河流 + 过路木桥（含桥墩） |
| 道路 | 十字路（南北 32m / 东西 38m），木桥跨河南北连通 |
| 山脉 | 6 座灰岩山体 + 雪帽，沿地图北缘形成山脉带 |
| 电力 | 5 根电线杆 + 横担 + 电线（其中 2 杆挂路灯，对应两盏点光源） |
| 植被/散石 | 12 树、10 灌木、岩石群 |
| 风车 | 叶轮持续自旋，杆顶挂聚光探照灯；顶部有公告板薄片始终朝向相机 |
| 灯光 | 环境光 1 + 太阳平行光 1 + 路灯点光 2 + 风车聚光 1（详见 `docs/灯光使用说明.md`） |

### 鼠标控制

| 操作 | 功能 |
|------|------|
| 左键拖动 | 旋转相机（绕目标点轨道旋转，Z 轴向上） |
| 右键拖动 | 平移相机（视平面内） |
| 中键拖动 | 缩放（沿 Z 轴） |
| 滚轮 | 缩放物体 |

### 键盘控制

| 按键 | 功能 |
|------|------|
| `←` / `→` | 左右旋转 |
| `↑` / `↓` | 上下旋转 |
| `W` / `S` | 沿 Y 轴水平前/后移动（-Y/+Y） |
| `A` / `D` | 沿 X 轴左/右移动 |
| `Q` / `E` | 沿 Z 轴下/上移动 |
| `+` / `=` | 放大 |
| `-` | 缩小 |
| `R` | 重置所有变换 |

## 技术特性

- **模块化架构**: 核心库 (core) 与平台层 (viewer) 分离，子系统经接口注入、可整体替换
- **可替换帧管线**: `IYalnFramePipeline` / `IYalnFramePass` 组织整帧与逻辑 Pass，默认 Forward；PSO 与材质分别经 `YalnPipelineLibrary` / `YalnMaterialLibrary` 注册，绘制项可按材质或按对象切换管线（详见 `docs/渲染管线接口使用说明.md`）
- **场景树渲染**: 组/叶/变换（位移、旋转、缩放、自旋）/开关/公告板节点，`update()` 每帧重算世界矩阵并收集绘制项
- **GPU 缓冲去重**: 同一网格被多处实例引用时只占一份显存（约 259 个绘制项仅 30 余份缓冲）
- **Vulkan 渲染管线**: 完整的图形管线初始化（实例、设备、交换链、渲染通道、深度测试等）
- **GLM 矩阵运算**: 模型、视图、投影矩阵变换
- **四元数旋转**: 节点、绘制项与相机朝向统一使用 Eigen 四元数，增量旋转无万向死锁
- **场景灯光系统**: 环境光/平行光/点光/聚光四类光源，场景持有并逐帧打包 UBO（动态光源上限 8 盏），片段着色器做 Blinn-Phong 光照，用屏幕空间导数求面法线（平面着色，无需顶点法线属性；详见 `docs/灯光系统设计说明.md`）
- **统一缓冲区 (UBO)**: 每帧动态更新变换矩阵、相机位置与灯光数据
- **索引缓冲区**: 高效渲染，重用顶点数据构建三角形面
- **网格系统**: 支持 YalnMesh 基类及 YalnCube（立方体）、YalnSphere（球体）、YalnPyramid（四棱锥），可选纯色填充
- **场景延迟加载**: 程序启动为空场景仅清屏，菜单加载后才构建场景树与 GPU 缓冲；修改场景树后经 `rebuildScene()` 重建
- **验证层**: Debug 模式下启用 Vulkan 验证层
- **着色器自动编译**: 构建时使用 glslc 自动编译 GLSL 着色器
- **交互控制**: 键盘 + 鼠标实时控制相机，菜单控制场景加载与自动旋转

## 核心 API

### 构建场景（YalnView 门面）

```cpp
// 创建网格（长、宽、高；最后一个参数可选实色，缺省为内置色板）
static YalnCube cube(1.0f, 1.0f, 1.0f, glm::vec3(0.91f, 0.87f, 0.78f));
static YalnSphere sphere(0.5f, 32);

// 场景树：根节点下挂绘制项或变换子树
IYalnScene& scene = view.scene();
auto leaf = std::make_unique<YalnLeafNode>("cube");
auto drawable = std::make_unique<YalnMeshDrawable>(&cube);
drawable->setPosition(Eigen::Vector3f(0.0f, 0.0f, 0.5f));
leaf->attachDrawable(std::move(drawable));
scene.root()->addChild(std::move(leaf));

// 修改场景树后重建 GPU 缓冲（内部先 waitIdle；同一网格多处引用只建一份缓冲）
view.rebuildScene();

// 检查缓冲区是否就绪
if (view.isMeshReady()) {
    // 可以开始渲染
}
```

### 添加灯光（yalnlight.h）

```cpp
#include "yalnlight.h"

auto ambient = std::make_unique<YalnAmbientLight>();
ambient->setColor(glm::vec3(0.58f, 0.65f, 0.78f));   // 偏冷天光
ambient->setIntensity(0.55f);
scene.addLight(std::move(ambient));

auto lamp = std::make_unique<YalnPointLight>();
lamp->setPosition(glm::vec3(0.0f, 2.4f, 1.62f));
lamp->setRange(8.0f);
lamp->setIntensity(1.3f);
scene.addLight(std::move(lamp));   // 修改属性下一帧生效；动态光源上限 8 盏
```

更多用法见 `docs/灯光使用说明.md`。

### 材质与自定义管线

```cpp
// 初始化并加载场景后：派生一条线框 PSO，再创建材质挂到节点
YalnPipelineConfig config = view.pipeline().config();   // 复制默认配置再改状态，保证布局兼容
config.polygonMode = vk::PolygonMode::eLine;
config.cullMode    = vk::CullModeFlagBits::eNone;
YalnPipelineHandle wirePipeline = view.createPipeline(config, "Wireframe");

YalnMaterialDesc desc;
desc.defaultPipeline = wirePipeline;
desc.baseColor       = glm::vec4(0.40f, 1.0f, 0.70f, 1.0f);
YalnMaterialHandle wireMaterial = view.createMaterial(desc);

if (YalnNode* node = scene.root()->find("mountain1")) {
    node->setMaterialRecursive(wireMaterial);
}
```

viewer 的「场景 → 应用材质/管线示例」菜单即使用上述路径；自定义配置需与当前渲染通道、描述符布局保持一致，更多扩展方式见 `docs/渲染管线接口使用说明.md`。

## License

MIT License
