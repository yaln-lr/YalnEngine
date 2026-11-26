#ifndef YALN_RENDER_MODE_CONTROLLER_H
#define YALN_RENDER_MODE_CONTROLLER_H

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "global_type.h"
#include "yalnrendertypes.h"
#include "yalnview.h"
#include "yalnmodematerial.h"

class YalnDrawable;
class YalnNode;

// ============================================================================
//  YalnRenderMode —— Viewer 的五种渲染模式
//
//    Normal       正常渲染（使用场景自身的材质颜色）
//    WhiteModel   白膜渲染（白膜材质 + 白膜 PSO，忽略顶点色）
//    Wireframe    网格渲染（线框材质 + 线框 PSO）
//    WhiteAndWire 白膜 + 网格叠加（白膜主材质 + 线框叠加材质）
//    NormalAndWire 正常渲染 + 网格叠加（原主材质 + 线框叠加材质）
//
//  切换只改 drawable 上的主材质句柄与叠加材质列表，不重建 GPU 资源，
//  不持有任何 Pass 指针或 enable 开关，下一帧生效。
// ============================================================================
enum class YalnRenderMode : uint8_t
{
    Normal,
    WhiteModel,
    Wireframe,
    WhiteAndWire,
    NormalAndWire
};

inline std::string_view toString(YalnRenderMode mode)
{
    switch (mode)
    {
        case YalnRenderMode::Normal:        return "正常渲染";
        case YalnRenderMode::WhiteModel:    return "白膜渲染";
        case YalnRenderMode::Wireframe:     return "网格渲染";
        case YalnRenderMode::WhiteAndWire:  return "白膜+网格";
        case YalnRenderMode::NormalAndWire: return "正常+网格";
    }
    return "未知";
}

// ============================================================================
//  YalnRenderModeController
//
//  持有 YalnView 引用，负责：
//    - 首次进入相应模式时创建白膜/线框 PSO 及引用它们的“模式材质”；
//    - 保存/恢复每个 drawable 的原始主材质与叠加材质列表；
//    - 按模式设置主材质与叠加材质。
//
//  场景重新加载后必须调用 refreshScene() 重新采集快照。
// ============================================================================
class YalnRenderModeController
{
public:
    explicit YalnRenderModeController(YalnView& view);
    ~YalnRenderModeController();

    YalnRenderMode mode() const { return m_mode; }
    std::string_view modeName() const { return toString(m_mode); }

    bool setMode(YalnRenderMode mode);

    // 场景重建后调用：重新收集 drawable 及其原始材质状态。
    void refreshScene();

private:
    struct DrawableState
    {
        YalnDrawable* drawable = nullptr;
        YalnMaterialHandle primary = YALN_INVALID_MATERIAL;
        std::vector<YalnMaterialHandle> overlays;
    };

    YalnModeMaterial* whiteMaterial();
    YalnModeMaterial* wireMaterial();
    void restoreDrawableStates();
    void setPrimaryAll(YalnMaterialHandle material);
    void addOverlayAll(YalnMaterialHandle material);
    static void collectDrawables(YalnNode* node, std::vector<YalnDrawable*>& out);

private:
    YalnView& m_view;
    YalnRenderMode m_mode = YalnRenderMode::Normal;

    // 特殊材质类：封装各自的 PSO / shader / 材质登记
    std::unique_ptr<YalnWhiteModelMaterial> m_whiteMaterial;
    std::unique_ptr<YalnWireframeMaterial>  m_wireMaterial;

    std::vector<DrawableState> m_originalStates;
};

#endif // YALN_RENDER_MODE_CONTROLLER_H
