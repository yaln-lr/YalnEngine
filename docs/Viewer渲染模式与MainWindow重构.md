# Viewer 渲染模式与 MainWindow 重构说明

## 1. 本次改动概览

| 需求 | 实现 |
|---|---|
| 五种渲染模式 + 任意切换 | `YalnRenderModeController` + 线框叠加 Pass，菜单「View → 渲染模式」 |
| 场景构建独立封装 | `YalnSceneFactory` / `YalnSceneAsset` |
| 降低 `main()` 复杂度 | `YalnMainWin`，`main()` 只剩 10 行装配代码 |
| ESC 退出 | `QtViewer::escapePressed` 信号 + `QShortcut` 兜底 |

新增文件：

```text
viewer/include/mainwin.h
viewer/include/yalnscenefactory.h
viewer/include/yalnrendermodecontroller.h
viewer/src/mainwin.cpp
viewer/src/yalnscenefactory.cpp
viewer/src/yalnrendermodecontroller.cpp
```

修改文件：

```text
core/include/yalninterfaces.h          （新增 depthBias 配置）
core/src/yalngraphicspipeline.cpp      （应用 depthBias 到 rasterizer）
core/include/yalnview.h / src/yalnview.cpp （新增 createPipeline(shaderProgram) 重载）
shader/shaders/white.frag              （新增白膜专用片元着色器）
shader/CMakeLists.txt                  （新增 white.frag.spv 编译目标）
viewer/src/main.cpp                    （极简化）
viewer/include/qtviewer.h / src/qtviewer.cpp  （新增 escapePressed 信号）
viewer/CMakeLists.txt                  （新增源文件）
```

---

## 2. 五种渲染模式

### 2.1 模式定义

定义在 `viewer/include/yalnrendermodecontroller.h`：

```cpp
enum class YalnRenderMode : uint8_t
{
    Normal,        // 正常渲染：使用场景自身的材质颜色
    WhiteModel,    // 白膜渲染：全部使用默认白色材质 + 默认管线
    Wireframe,     // 网格渲染：保留原材质颜色，强制使用线框 PSO
    WhiteAndWire,  // 白膜 + 网格叠加：先画白膜，再叠加一层线框
    NormalAndWire  // 正常 + 网格叠加：原色填充，再叠加一层线框
};
```

### 2.2 实现矩阵

| 模式 | 填充 | 管线 | 叠加 Pass |
|---|---|---|---|
| Normal | 原顶点色 | 原管线 | 关闭 |
| WhiteModel | 白膜 shader（忽略顶点色） | 白膜 PSO（`pipelineOverride`） | 关闭 |
| Wireframe | 原顶点色 | 线框 PSO（`pipelineOverride`） | 关闭 |
| WhiteAndWire | 白膜 shader | 白膜 PSO（`pipelineOverride`） | `YalnWireframeOverlayPass`（开启） |
| NormalAndWire | 原顶点色（正常材质） | 原管线 | `YalnWireframeOverlayPass`（开启） |

`NormalAndWire`（正常渲染 + 网格）保留物体的原始顶点色/材质作为填充，再叠加一层线框，适合查看带颜色模型的网格结构；它不需要白膜 PSO，只需要线框 PSO 和叠加 Pass。

> 早期版本曾用“默认白色材质”实现白膜，但因为物体颜色来自**顶点色**（`albedo = fragColor * base_color`），白色乘色不会改变顶点色，导致白膜与正常渲染完全一样。现已改为独立 shader 方案。

### 2.3 关键技术点

**线框 PSO（带深度偏移）**

首次进入线框相关模式时，`ensureWirePipeline()` 以默认配置为模板创建：

```cpp
config.polygonMode = vk::PolygonMode::eLine;
config.cullMode = vk::CullModeFlagBits::eNone;
config.depthBiasEnable = true;
config.depthBiasConstantFactor = -1.5f;
config.depthBiasSlopeFactor = -1.5f;
config.depthWrite = false;
```

为此在 `YalnPipelineConfig` 新增了 4 个字段：

```cpp
bool  depthBiasEnable;
float depthBiasConstantFactor;
float depthBiasClamp;
float depthBiasSlopeFactor;
```

`YalnGraphicsPipeline::create()` 把它们写入 `vk::PipelineRasterizationStateCreateInfo`。负的深度偏移让线框深度更靠近相机，配合关闭深度写入，保证线框稳定显示在白膜之上，不产生 z-fighting。

**白膜 PSO（独立 shader）**

新增 `shader/shaders/white.frag`：与 `shader.frag` 接口和 Blinn-Phong 光照完全一致，唯一区别是把反照率固定为中性白 `vec3(0.85)`，**不使用顶点色**。顶点着色器仍复用 `shader.vert`。

为此给 `YalnView` 增加了重载：

```cpp
YalnPipelineHandle createPipeline(const YalnPipelineConfig& config,
                                  const IYalnShaderProgram& shaderProgram,
                                  const std::string& name = {});
```

控制器持有一个 `YalnSpirvShaderProgram("../shaders/shader.vert.spv", "../shaders/white.frag.spv")`，用它创建白膜 PSO（句柄 1）。`shader/CMakeLists.txt` 增加了 `white.frag.spv` 的编译目标。

白膜模式通过 `pipelineOverride = 白膜 PSO` 让 Opaque Pass 使用白膜 shader；由于 shader 不读顶点色也不读 `base_color`，材质保持原样即可。`WhiteAndWire` 模式的白膜填充同样使用该 PSO。

**白膜 + 网格叠加 Pass**

`YalnWireframeOverlayPass` 是 `IYalnFramePass` 的实现，追加到 `YalnForwardFramePipeline` 的 Pass 列表末尾（Opaque 之后）。它：

- 遍历 `context.scene->drawItems()`；
- 固定使用线框 PSO；
- push constants 使用青绿色 `baseColor`；
- 通过 `setEnabled(bool)` 控制是否录制，仅 `WhiteAndWire` / `NormalAndWire` 模式开启。

该 Pass 由 FramePipeline 拥有，控制器只持有控制接口裸指针。

**切换开销**

切换只修改 drawable 上的材质句柄 / `pipelineOverride` 和叠加 Pass 的 enable 标志，不重建任何 GPU 资源。下一帧 `scene.update()` 重建 drawItems 时自动生效。

**原始材质保存/恢复**

控制器在 `refreshScene()` 中递归收集所有 drawable 的材质句柄。每次切换先 `restoreDrawableStates()` 回到原始状态，再按目标模式设置，保证任意顺序切换都能正确还原。

### 2.4 使用方式

菜单：`View → 渲染模式 → 正常渲染 / 白膜渲染 / 网格渲染 / 白膜+网格 / 正常+网格`

命令行（便于自动化）：

```powershell
.\yalnviewer.exe --load-village --mode=0   # 正常
.\yalnviewer.exe --load-village --mode=1   # 白膜
.\yalnviewer.exe --load-village --mode=2   # 网格
.\yalnviewer.exe --load-village --mode=3   # 白膜+网格
.\yalnviewer.exe --load-village --mode=4   # 正常+网格
```

注意：模式切换需要场景已加载；空场景时切换会被拒绝并给出日志提示。

---

## 3. 场景工厂

```cpp
struct YalnSceneAsset
{
    std::unique_ptr<YalnNode>               root;
    std::vector<std::unique_ptr<YalnMesh>>  meshes;
    std::vector<std::unique_ptr<YalnLight>> lights;
};

class YalnSceneFactory
{
public:
    static YalnSceneAsset createVillage();
    static YalnSceneAsset createEmpty();
};
```

设计要点：

- `YalnMesh` 在场景树中只以裸指针被引用，所以网格所有权必须由 `YalnSceneAsset` 持有；
- 灯光由 asset 收集，调用方再逐个移入 `IYalnScene`；
- 工厂不接触 Vulkan / Renderer，只产出纯数据；
- `YalnMainWin::onLoadVillage()` 负责：
  `waitIdle → createVillage → scene.clear → setRoot → addLight → 持有 asset → rebuildScene`。

加载后的场景统计：

```text
Scene factory: nodes=307, drawables=259, meshes=32, lights=5
```

---

## 4. YalnMainWin

`main()` 现在只有装配代码：

```cpp
int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    YalnMainWin window;
    if (!window.initializeVulkan("Yaln Vulkan Viewer")) return 1;
    return app.exec();
}
```

`YalnMainWin` 承担：

- 中央区域布局（QtViewer + 状态栏）；
- 菜单：File / View（含渲染模式子菜单）/ 场景 / Help；
- 状态栏与 FPS 统计；
- Vulkan 配置注入与初始化（`initializeVulkan()`）；
- 相机设置；
- 场景加载与模式切换的槽函数；
- 命令行参数解析。

---

## 5. ESC 退出

问题：`QtViewer::keyPressEvent()` 调用 `event->accept()`，阻止按键向父窗口传播，导致 `QShortcut` 收不到 Esc。

解决：双保险。

1. 在 `QtViewer` 中新增 `escapePressed()` 信号，`keyPressEvent` 检测 `Qt::Key_Escape` 时直接 emit；
2. `YalnMainWin` 连接该信号到 `onQuit()`（`close()`）；
3. 同时保留 `QShortcut(ApplicationShortcut)` 作为 viewer 无焦点时的兜底。

```cpp
void QtViewer::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape)
    {
        emit escapePressed();
        event->accept();
        return;
    }
    handleKeyInput(event->key(), true);
    event->accept();
}
```

---

## 6. 命令行参数汇总

| 参数 | 作用 |
|---|---|
| `--load-village` | 启动时自动加载村庄场景 |
| `--demo-render-extension` | 加载场景并应用旧的材质/管线示例（风车暖色） |
| `--mode=0..4` | 启动后切换到指定渲染模式（0 正常 / 1 白膜 / 2 网格 / 3 白膜+网格 / 4 正常+网格） |
| `--quit-after=秒` | 自动化用：N 秒后调用与 ESC 相同的 `onQuit()` 退出 |

---

## 7. 验证结果

- Debug 构建通过；
- 五种模式（正常 / 白膜 / 网格 / 白膜+网格 / 正常+网格）分别运行 8~10 秒，Vulkan 校验层无 VUID / Validation Error；
- `--quit-after` 触发的 `close()` → 场景树清理 → `waitIdle` → renderer shutdown 流程正常结束。

注意：窗口关闭阶段可能观察到交换链重建的校验日志（窗口尺寸归零导致），这是既有行为，与本次改动无关。
