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
viewer/include/yalnmodematerial.h / src/yalnmodematerial.cpp （特殊材质类）
core/include/yalndrawable.h            （主材质 / 叠加材质列表 / 递归接口）
core/include/yalnnode.h                （叠加材质递归辅助）
core/include/yalninterfaces.h          （YalnDrawItem 增加 overlayMaterials）
core/src/yalnscene.cpp                 （每帧收集叠加材质快照）
core/src/yalnforwardpass.cpp           （OpaquePass + 通用叠加材质 Pass 共用批处理）
core/src/yalnforwardpipeline.cpp       （默认追加 YalnMaterialOverlayPass(Outline)）
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

### 2.2 实现矩阵（方案 A + 材质驱动）

模式切换完全由 drawable 上的材质驱动，控制器不再持有 `pipelineOverride` 调用或任何 Pass 指针：

| 模式 | 主材质 | 叠加材质 |
|---|---|---|
| Normal | 原材质 | 清空 |
| WhiteModel | `Mode.White`（白膜材质） | 清空 |
| Wireframe | `Mode.Wire`（线框材质） | 清空 |
| WhiteAndWire | `Mode.White` | 添加 `Mode.Wire` |
| NormalAndWire | 原材质 | 添加 `Mode.Wire` |

**特殊材质类（viewer/include/yalnmodematerial.h）**

白膜与网格材质已封装成独立的特殊材质类，控制器不再直接创建 PSO / shader / 材质：

```cpp
class YalnModeMaterial                  // 抽象基类：持有 YalnView&、PSO 句柄、材质句柄
{
public:
    virtual bool create() = 0;          // 懒创建，幂等
    YalnMaterialHandle handle() const;
    YalnPipelineHandle pipeline() const;
};

class YalnWhiteModelMaterial : public YalnModeMaterial  // 白膜：白膜 shader + PSO + 材质
class YalnWireframeMaterial  : public YalnModeMaterial  // 网格：线框 PSO + 材质
```

这是包装类而非 `YalnMaterial` 子类——后者由材质库以 `unique_ptr<YalnMaterial>` 持有且析构非虚，不能安全继承。特殊材质类负责创建专用 PSO、登记材质，对外只暴露句柄。

- `Mode.White` 引用白膜 PSO（白膜 shader，忽略顶点色），`passMask = Opaque`；
- `Mode.Wire` 引用线框 PSO（depthBias + 不写深度），`passMask = Opaque | Outline`，因此既能作主材质画线框，也能作叠加材质叠加；
- 默认 Forward 的 Pass 列表为两个逻辑 Pass：`YalnForwardOpaquePass`（画 `item.material`）+ `YalnMaterialOverlayPass(Outline)`（画 `item.overlayMaterials[]`）；
- 两个 Pass 共享同一份批处理实现（core/src/yalnforwardpass.cpp），OpaquePass 主材质路径仍 honor 兼容保留的 `pipelineOverride`，叠加材质路径完全由材质决定；
- 叠加 Pass 常驻且无 enable 开关：没有叠加材质时它不录任何命令。

> 早期版本曾用“默认白色材质”实现白膜，但因为物体颜色来自**顶点色**（`albedo = fragColor * base_color`），白色乘色不会改变顶点色，导致白膜与正常渲染完全一样。现已改为独立 shader 方案。

### 2.3 关键技术点

**线框 PSO（带深度偏移）**

首次进入线框相关模式时，`YalnWireframeMaterial::create()` 以默认配置为模板创建：

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

`YalnWhiteModelMaterial` 持有一个 `YalnSpirvShaderProgram("../shaders/shader.vert.spv", "../shaders/white.frag.spv")`，`create()` 用它创建白膜 PSO 并登记 `Mode.White` 材质。`shader/CMakeLists.txt` 增加了 `white.frag.spv` 的编译目标。

白膜模式把所有 drawable 的主材质换成 `Mode.White`（引用白膜 PSO），Opaque Pass 因此走白膜 shader；shader 不读顶点色也不读 `base_color`。`WhiteAndWire` 模式同样以它作主材质。

**白膜 + 网格叠加 Pass**

叠加绘制由 `YalnMaterialOverlayPass(YalnPassKind::Outline)` 完成，它在 `YalnForwardFramePipeline` 构造时就常驻在 Opaque 之后，没有 enable 开关。它：

- 遍历 `context.scene->drawItems()` 的 `overlayMaterials[]`；
- 每个叠加材质按 `pipelineForPass(Outline)` 解析管线（线框材质即线框 PSO）；
- push constants 使用材质自身的 `baseColor`（线框为青绿色）；
- 没有任何叠加材质时解析结果为空，不录制任何绘制命令。

控制器不持有 Pass 指针：`WhiteAndWire` / `NormalAndWire` 只是给 drawable 的叠加材质列表加上 `Mode.Wire`。

**切换开销**

切换只修改 drawable 的主材质句柄与叠加材质列表，不重建任何 GPU 资源。下一帧 `scene.update()` 重建 drawItems 时自动生效。

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
