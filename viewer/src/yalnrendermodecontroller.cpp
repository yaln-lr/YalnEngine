#include "../include/yalnrendermodecontroller.h"

#include "yalndrawable.h"
#include "yalnnode.h"
#include "yalnrendertypes.h"

#include <algorithm>
#include <iostream>
#include <vector>

// ============================================================================
//  YalnRenderModeController 实现
//
//  方案 A + 特殊材质类：
//    - 白膜 / 线框材质封装为 YalnWhiteModelMaterial / YalnWireframeMaterial；
//    - 控制器不持有任何 Pass 指针 / enable 开关，也不直接创建 PSO/shader；
//    - 模式切换只改 drawable 的主材质句柄与叠加材质列表；
//    - 叠加绘制由 ForwardFramePipeline 中常驻的
//      YalnMaterialOverlayPass(Outline) 按 item.overlayMaterials[] 完成。
// ============================================================================

YalnRenderModeController::YalnRenderModeController(YalnView& view)
    : m_view(view)
{
}

YalnRenderModeController::~YalnRenderModeController() = default;

void YalnRenderModeController::collectDrawables(YalnNode* node,
                                                std::vector<YalnDrawable*>& out)
{
    if (node == nullptr)
    {
        return;
    }
    for (const auto& drawable : node->drawables())
    {
        if (drawable != nullptr)
        {
            out.push_back(drawable.get());
        }
    }
    for (const auto& child : node->children())
    {
        if (child != nullptr)
        {
            collectDrawables(child.get(), out);
        }
    }
}

void YalnRenderModeController::refreshScene()
{
    std::vector<YalnDrawable*> drawables;
    collectDrawables(m_view.scene().root(), drawables);

    m_originalStates.clear();
    m_originalStates.reserve(drawables.size());
    for (YalnDrawable* drawable : drawables)
    {
        DrawableState state;
        state.drawable = drawable;
        state.primary = drawable->material();
        state.overlays = drawable->overlayMaterials();
        m_originalStates.push_back(std::move(state));
    }
}

YalnModeMaterial* YalnRenderModeController::whiteMaterial()
{
    if (!m_whiteMaterial)
    {
        m_whiteMaterial = std::make_unique<YalnWhiteModelMaterial>(m_view);
    }
    if (!m_whiteMaterial->isReady() && !m_whiteMaterial->create())
    {
        return nullptr;
    }
    return m_whiteMaterial.get();
}

YalnModeMaterial* YalnRenderModeController::wireMaterial()
{
    if (!m_wireMaterial)
    {
        m_wireMaterial = std::make_unique<YalnWireframeMaterial>(m_view);
    }
    if (!m_wireMaterial->isReady() && !m_wireMaterial->create())
    {
        return nullptr;
    }
    return m_wireMaterial.get();
}

void YalnRenderModeController::restoreDrawableStates()
{
    for (const DrawableState& state : m_originalStates)
    {
        if (state.drawable == nullptr)
        {
            continue;
        }
        state.drawable->setMaterial(state.primary);
        state.drawable->clearOverlayMaterials();
        for (const YalnMaterialHandle handle : state.overlays)
        {
            state.drawable->addOverlayMaterial(handle);
        }
    }
}

void YalnRenderModeController::setPrimaryAll(YalnMaterialHandle material)
{
    for (const DrawableState& state : m_originalStates)
    {
        if (state.drawable != nullptr)
        {
            state.drawable->setMaterial(material);
        }
    }
}

void YalnRenderModeController::addOverlayAll(YalnMaterialHandle material)
{
    for (const DrawableState& state : m_originalStates)
    {
        if (state.drawable != nullptr)
        {
            state.drawable->addOverlayMaterial(material);
        }
    }
}

bool YalnRenderModeController::setMode(YalnRenderMode mode)
{
    if (!m_view.isInitialized() || m_view.scene().isEmpty())
    {
        std::cerr << "[RenderMode] 未初始化或场景为空，无法切换渲染模式" << std::endl;
        return false;
    }

    // 解析目标模式需要的特殊材质（内部懒创建 PSO + 材质）
    YalnModeMaterial* white = nullptr;
    YalnModeMaterial* wire = nullptr;

    if (mode == YalnRenderMode::WhiteModel || mode == YalnRenderMode::WhiteAndWire)
    {
        white = whiteMaterial();
        if (white == nullptr)
        {
            std::cerr << "[RenderMode] 白膜材质不可用，保持原模式" << std::endl;
            return false;
        }
    }
    if (mode == YalnRenderMode::Wireframe ||
        mode == YalnRenderMode::WhiteAndWire ||
        mode == YalnRenderMode::NormalAndWire)
    {
        wire = wireMaterial();
        if (wire == nullptr)
        {
            std::cerr << "[RenderMode] 线框材质不可用，保持原模式" << std::endl;
            return false;
        }
    }

    restoreDrawableStates();

    switch (mode)
    {
        case YalnRenderMode::Normal:
            break;

        case YalnRenderMode::WhiteModel:
            setPrimaryAll(white->handle());
            break;

        case YalnRenderMode::Wireframe:
            setPrimaryAll(wire->handle());
            break;

        case YalnRenderMode::WhiteAndWire:
            setPrimaryAll(white->handle());
            addOverlayAll(wire->handle());
            break;

        case YalnRenderMode::NormalAndWire:
            addOverlayAll(wire->handle());
            break;
    }

    m_mode = mode;
    std::cout << "[RenderMode] 切换到: " << toString(mode)
              << " (whiteMat=" << (white ? white->handle() : YALN_INVALID_MATERIAL)
              << ", wireMat=" << (wire ? wire->handle() : YALN_INVALID_MATERIAL) << ")"
              << std::endl;
    return true;
}
