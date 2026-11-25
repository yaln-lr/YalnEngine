#ifndef YALN_RENDER_MODE_CONTROLLER_H
#define YALN_RENDER_MODE_CONTROLLER_H

#include <memory>
#include <utility>
#include <vector>

#include "global_type.h"
#include "yalnrendertypes.h"
#include "yalnview.h"
#include "yalnshaderprogram.h"

#include <memory>

class YalnDrawable;
class YalnNode;

// 线框叠加 Pass 的运行期控制接口（具体实现在 .cpp 中）
class IWireframeOverlayControl
{
public:
    virtual ~IWireframeOverlayControl() = default;
    virtual void setEnabled(bool enabled) = 0;
    virtual void setPipeline(YalnPipelineHandle pipeline) = 0;
};

// ============================================================================
//  YalnRenderMode —— Viewer 的五种渲染模式
//
//    Normal       正常渲染（使用场景自身的材质颜色）
//    WhiteModel   白膜渲染（所有对象强制使用默认白色材质 + 默认管线）
//    Wireframe    网格渲染（所有对象保留原材质，但强制使用线框 PSO）
//    WhiteAndWire 白膜 + 网格叠加（先画白膜，再叠加一层线框）
//    NormalAndWire 正常渲染 + 网格叠加（保留顶点色/材质，再叠加一层线框）
//
//  切换只改变 drawable 上的材质句柄 / pipelineOverride，以及叠加 Pass 的开关，
//  不重建任何 GPU 资源，因此可以在任意时刻切换，下一帧生效。
// ============================================================================
enum class YalnRenderMode : uint8_t
{
    Normal,
    WhiteModel,
    Wireframe,
    WhiteAndWire,
    NormalAndWire
};

inline const char* toString(YalnRenderMode mode)
{
    switch (mode)
    {
        case YalnRenderMode::Normal:       return "正常渲染";
        case YalnRenderMode::WhiteModel:   return "白膜渲染";
        case YalnRenderMode::Wireframe:    return "网格渲染";
        case YalnRenderMode::WhiteAndWire: return "白膜+网格";
        case YalnRenderMode::NormalAndWire: return "正常+网格";
    }
    return "未知";
}

// ============================================================================
//  YalnRenderModeController
//
//  持有 YalnView 引用，负责：
//    - 首次进入线框相关模式时创建线框 PSO（带 depthBias，避免 z-fighting）；
//    - 保存/恢复每个 drawable 的原始材质，保证切回 Normal 完全还原；
//    - 按需向默认 Forward FramePipeline 追加一个线框叠加 Pass。
//
//  场景被重新加载后必须调用 refreshScene() 重新采集 drawable 与原始材质。
// ============================================================================
class YalnRenderModeController
{
public:
    explicit YalnRenderModeController(YalnView& view);
    ~YalnRenderModeController();

    YalnRenderMode mode() const { return m_mode; }
    const char* modeName() const { return toString(m_mode); }

    bool setMode(YalnRenderMode mode);

    // 场景重建后调用：重新收集 drawable 并刷新原始材质快照。
    void refreshScene();

private:
    void ensureWirePipeline();
    void ensureWhitePipeline();
    void restoreDrawableStates();
    void addOverlayPassIfNeeded();
    void setOverlayEnabled(bool enabled);
    static void collectDrawables(YalnNode* node, std::vector<YalnDrawable*>& out);

private:
    YalnView& m_view;
    YalnRenderMode m_mode = YalnRenderMode::Normal;

    YalnPipelineHandle m_wirePipeline = YALN_INVALID_PIPELINE;
    YalnPipelineHandle m_whitePipeline = YALN_INVALID_PIPELINE;

    // 白膜 shader program 由控制器持有（管线只引用，不接管）
    std::unique_ptr<YalnSpirvShaderProgram> m_whiteShaderProgram;

    std::vector<std::pair<YalnDrawable*, YalnMaterialHandle>> m_originalMaterials;

    // 叠加 Pass 由 FramePipeline 拥有，这里只保存控制接口裸指针
    IWireframeOverlayControl* m_overlayPass = nullptr;
    bool m_overlayAdded = false;
};

#endif // YALN_RENDER_MODE_CONTROLLER_H
