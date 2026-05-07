# VulkanMaster

Vulkan 学习项目，包含多种窗口后端实现和 C/C++ API 版本。

## 项目结构

```
YalnEngine/
├── CMakeLists.txt              # 根 CMake（公共依赖、configure_vulkan 函数、着色器编译）
├── core/                       # 核心渲染库
│   ├── include/                # 头文件
│   │   ├── yalnview.h          # 视图基类（渲染逻辑）
│   │   ├── yalnmesh.h          # 网格数据（YalnMesh/YalnCube/YalnSphere）
│   │   ├── yalncamera.h        # 相机基类
│   │   └── yalnperspectivecamera.h  # 透视相机
│   └── src/                    # 实现文件
├── example/                    # 示例应用
│   └── glfwViewer/             # GLFW 窗口示例
│       ├── include/
│       └── src/
├── shader/shaders/             # 共享着色器（所有子项目共用）
│   ├── shader.vert             # 顶点着色器（GLSL）
│   ├── shader.frag             # 片段着色器（GLSL）
│   ├── shader.vert.spv         # 编译后的顶点着色器
│   └── shader.frag.spv         # 编译后的片段着色器
├── thirdparty/                 # 第三方依赖（DLL 等）
├── build/                      # 构建输出目录
│   └── output/
│       ├── bin/                # 可执行文件
│       └── shaders/            # 安装的着色器文件
├── KEYBOARD_CONTROLS.md        # 键盘控制说明
└── README.md
```

## 环境依赖

| 依赖 | 版本 | 用途 |
|------|------|------|
| CMake | >= 3.10 | 构建系统 |
| C++ | C++17 | 语言标准 |
| Qt6 | 6.11.0 (mingw_64) | 窗口管理（qwidget_app / qwindow_app） |
| GLFW | 3.4 | 窗口管理（glfw_app / glfwViewer） |
| Vulkan SDK | 1.4.341.1 | 图形 API |
| GLM | latest | 数学库（矩阵/向量运算） |
| MinGW-w64 (UCRT64) | GCC 15+ | 编译器 |
| glslc | - | 着色器编译器 |

## 构建与运行

### 配置

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=d:/QT6/6.11.0/mingw_64
```

### 编译

```powershell
cmake --build build -j 18
```

### 安装

```powershell
cmake --install build
```

输出到 `build/output/bin/`，包含 `glfwViewer.exe` 等可执行文件。

### VS Code

- **编译**: `Ctrl+Shift+B`
- **调试**: `F5`（自动编译+安装后启动 GDB 调试）
- 工作目录设为 `build/output/`，确保着色器路径正确

## 操作说明

### 鼠标控制

| 操作 | 功能 |
|------|------|
| 左键拖动 | 旋转相机（绕 X/Y 轴） |
| 右键拖动 | 平移相机（X/Y 轴） |
| 中键拖动 | 缩放（沿 Z 轴） |
| 滚轮 | 缩放物体 |

### 键盘控制

| 按键 | 功能 |
|------|------|
| `←` / `→` | 左右旋转 |
| `↑` / `↓` | 上下旋转 |
| `W` / `S` | 沿 Z 轴前/后移动 |
| `A` / `D` | 沿 X 轴左/右移动 |
| `Q` / `E` | 沿 Y 轴下/上移动 |
| `+` / `=` | 放大 |
| `-` | 缩小 |
| `R` | 重置所有变换 |

## 技术特性

- **模块化架构**: 核心库 (core) 与示例应用 (example) 分离
- **Vulkan 渲染管线**: 完整的图形管线初始化（实例、设备、交换链、渲染通道等）
- **GLM 矩阵运算**: 模型、视图、投影矩阵变换
- **统一缓冲区 (UBO)**: 每帧动态更新变换矩阵
- **索引缓冲区**: 高效渲染，重用顶点数据构建三角形面
- **网格系统**: 支持 YalnMesh 基类及 YalnCube（立方体）、YalnSphere（球体）等派生类
- **延迟加载**: 顶点/索引缓冲区通过 `setMesh()` 延迟创建，按需分配 GPU 资源
- **验证层**: Debug 模式下启用 Vulkan 验证层
- **着色器自动编译**: 构建时使用 glslc 自动编译 GLSL 着色器
- **交互控制**: 键盘 + 鼠标实时控制物体变换

## 核心 API

### 设置网格

```cpp
// 创建立方体网格（长、宽、高）
YalnCube cube(1.0f, 1.0f, 1.0f);

// 创建球体网格（半径、分段数）
YalnSphere sphere(0.5f, 32);

// 设置到视图（自动创建 GPU 缓冲区）
view.setMesh(&cube);

// 检查缓冲区是否就绪
if (view.isMeshReady()) {
    // 可以开始渲染
}
```

## License

MIT License
