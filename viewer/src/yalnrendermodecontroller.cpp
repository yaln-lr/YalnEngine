#include "../include/yalnrendermodecontroller.h"

#include "yalnforwardpipeline.h"
#include "yalnframe.h"
#include "yalnmateriallibrary.h"
#include "yalnnode.h"
#include "yalndrawable.h"
#include "yalninterfaces.h"
#include "yalnrendertypes.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <vector>

// ============================================================================
//  线框叠加 Pass（内部实现）
//
//  在默认 Forward 的 Opaque Pass 之后录制：遍历 drawItems，固定使用
//  带 depthBias 的线框 PSO 再画一遍，使线框显示在白膜（实体）之上。
//  enable 标志由 YalnRenderModeController 控制，仅在 WhiteAndWire 模式打开。
// ============================================================================
class YalnWireframeOverlayPass : public IYalnFramePass,
                                  public IWireframeOverlayControl
{
public:
    explicit YalnWireframeOverlayPass(YalnPipelineHandle pipeline)
        : m_pipeline(pipeline)
    {
    }

    const char* name() const override { return "Forward.WireframeOverlay"; }
    uint32_t    passMask() const override { return toPassMask(YalnPassKind::Outline); }

    void setPipeline(YalnPipelineHandle pipeline) { m_pipeline = pipeline; }
    void setEnabled(bool enabled)                 { m_enabled = enabled; }
    bool enabled() const                          { return m_enabled; }

    void record(const YalnFrameContext& context) override
    {
        if (!m_enabled ||
            context.commandBuffer == VK_NULL_HANDLE ||
            context.scene == nullptr ||
            context.pipelines == nullptr ||
            m_pipeline == YALN_INVALID_PIPELINE)
        {
            return;
        }

        const IYalnPipeline* pipeline = context.pipelines->get(m_pipeline);
        if (pipeline == nullptr || !pipeline->isValid())
        {
            return;
        }

        const uint32_t passBit = passMask();
        struct Entry { const YalnDrawItem* item; };
        std::vector<Entry> items;
        for (const YalnDrawItem& item : context.scene->drawItems())
        {
            if (item.gpuMesh == nullptr || (item.passMask & passBit) == 0)
            {
                continue;
            }
            items.push_back({ &item });
        }
        if (items.empty())
        {
            return;
        }

        vk::CommandBuffer cmd = context.commandBuffer;
        const IYalnPipeline* boundPipeline = nullptr;

        YalnMaterialPushConstants push{};
        push.baseColor = m_color;  // 线框颜色（world 每次按绘制项覆盖）

        for (const Entry& entry : items)
        {
            const YalnDrawItem& item = *entry.item;

            if (boundPipeline != pipeline)
            {
                cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline->handle());
                if (pipeline->hasDescriptorSets() &&
                    context.frameDescriptorSet != VK_NULL_HANDLE)
                {
                    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                           pipeline->layout(), 0,
                                           { context.frameDescriptorSet }, {});
                }
                boundPipeline = pipeline;
            }

            push.world = item.world;
            if (pipeline->hasPushConstants())
            {
                const vk::PushConstantRange& range = pipeline->config().pushConstant;
                const uint32_t bytes = std::min<uint32_t>(
                    range.size, sizeof(YalnMaterialPushConstants));
                if (range.offset == 0 && bytes >= sizeof(glm::mat4))
                {
                    cmd.pushConstants(
                        pipeline->layout(), range.stageFlags, 0,
                        vk::ArrayProxy<const uint8_t>(
                            bytes, reinterpret_cast<const uint8_t*>(&push)));
                }
            }

            cmd.bindVertexBuffers(0, { item.gpuMesh->vertexBuffer.handle() }, { 0 });
            cmd.bindIndexBuffer(item.gpuMesh->indexBuffer.handle(), 0,
                                vk::IndexType::eUint32);
            cmd.drawIndexed(item.gpuMesh->indexCount, 1, 0, 0, 0);
        }
    }

private:
    YalnPipelineHandle m_pipeline = YALN_INVALID_PIPELINE;
    bool  m_enabled = false;
    glm::vec4 m_color{ 0.35f, 1.0f, 0.65f, 1.0f };  // 青绿色线框
};

// ============================================================================
//  YalnRenderModeController 实现
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

    m_originalMaterials.clear();
    m_originalMaterials.reserve(drawables.size());
    for (YalnDrawable* drawable : drawables)
    {
        m_originalMaterials.emplace_back(drawable, drawable->material());
    }
}

void YalnRenderModeController::ensureWirePipeline()
{
    if (m_wirePipeline != YALN_INVALID_PIPELINE)
    {
        return;
    }

    if (!m_view.device().physicalDevice().getFeatures().fillModeNonSolid)
    {
        std::cerr << "[RenderMode] 设备不支持 fillModeNonSolid，线框模式不可用" << std::endl;
        return;
    }

    YalnPipelineConfig config = m_view.pipeline().config();
    config.polygonMode = vk::PolygonMode::eLine;
    config.cullMode = vk::CullModeFlagBits::eNone;
    config.lineWidth = 1.0f;

    // 线框叠加在白膜之上：关闭深度写入 + 负深度偏移，避免与实体共面 z-fighting。
    config.depthBiasEnable = true;
    config.depthBiasConstantFactor = -1.5f;
    config.depthBiasSlopeFactor = -1.5f;
    config.depthWrite = false;

    m_wirePipeline = m_view.createPipeline(config, "RenderMode.Wireframe");
    if (m_wirePipeline == YALN_INVALID_PIPELINE)
    {
        std::cerr << "[RenderMode] 创建线框 PSO 失败" << std::endl;
    }
}

void YalnRenderModeController::ensureWhitePipeline()
{
    if (m_whitePipeline != YALN_INVALID_PIPELINE)
    {
        return;
    }

    // 白膜 shader：复用默认顶点着色器 + 白膜专用片元着色器。
    // 路径相对于 viewer 运行目录（build/mingw/viewer）。
    m_whiteShaderProgram = std::make_unique<YalnSpirvShaderProgram>(
        "../shaders/shader.vert.spv",
        "../shaders/white.frag.spv");
    if (!m_whiteShaderProgram->load(m_view.device()))
    {
        std::cerr << "[RenderMode] 白膜着色器加载失败" << std::endl;
        m_whiteShaderProgram.reset();
        return;
    }

    // 顶点输入布局、深度状态等全部继承默认配置，只换 shader。
    YalnPipelineConfig config = m_view.pipeline().config();
    m_whitePipeline = m_view.createPipeline(config, *m_whiteShaderProgram,
                                            "RenderMode.WhiteModel");
    if (m_whitePipeline == YALN_INVALID_PIPELINE)
    {
        std::cerr << "[RenderMode] 创建白膜 PSO 失败" << std::endl;
    }
}

void YalnRenderModeController::addOverlayPassIfNeeded()
{
    if (m_overlayAdded)
    {
        return;
    }

    auto* forward = dynamic_cast<YalnForwardFramePipeline*>(m_view.framePipeline());
    if (forward == nullptr)
    {
        std::cerr << "[RenderMode] 当前整帧策略不是默认 Forward，无法追加线框叠加 Pass" << std::endl;
        return;
    }

    auto pass = std::make_unique<YalnWireframeOverlayPass>(m_wirePipeline);
    m_overlayPass = pass.get();
    forward->addPass(std::move(pass));
    m_overlayAdded = true;
}

void YalnRenderModeController::setOverlayEnabled(bool enabled)
{
    if (m_overlayPass != nullptr)
    {
        m_overlayPass->setEnabled(enabled);
    }
    else if (enabled)
    {
        addOverlayPassIfNeeded();
        setOverlayEnabled(enabled);
    }
}

void YalnRenderModeController::restoreDrawableStates()
{
    for (const auto& entry : m_originalMaterials)
    {
        if (entry.first != nullptr)
        {
            entry.first->setMaterial(entry.second);
            entry.first->setPipelineOverride(YALN_INVALID_PIPELINE);
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

    // 线框模式需要先确保 PSO 可用
    if (mode == YalnRenderMode::Wireframe ||
        mode == YalnRenderMode::WhiteAndWire ||
        mode == YalnRenderMode::NormalAndWire)
    {
        ensureWirePipeline();
        if (m_wirePipeline == YALN_INVALID_PIPELINE)
        {
            std::cerr << "[RenderMode] 线框 PSO 不可用，保持原模式" << std::endl;
            return false;
        }
    }

    // 白膜模式需要独立的白膜 PSO（忽略顶点色的 shader）
    if (mode == YalnRenderMode::WhiteModel || mode == YalnRenderMode::WhiteAndWire)
    {
        ensureWhitePipeline();
        if (m_whitePipeline == YALN_INVALID_PIPELINE)
        {
            std::cerr << "[RenderMode] 白膜 PSO 不可用，保持原模式" << std::endl;
            return false;
        }
    }

    // 先还原到原始状态，再按目标模式重新设置
    restoreDrawableStates();
    setOverlayEnabled(false);

    switch (mode)
    {
        case YalnRenderMode::Normal:
            // 已还原，无需额外操作
            break;

        case YalnRenderMode::WhiteModel:
            // 白膜 PSO 使用忽略顶点色的 shader，直接覆盖管线选择即可。
            // 材质保持原样（白膜 shader 不读取 base_color / 顶点色）。
            for (const auto& entry : m_originalMaterials)
            {
                if (entry.first != nullptr)
                {
                    entry.first->setPipelineOverride(m_whitePipeline);
                }
            }
            break;

        case YalnRenderMode::Wireframe:
            // 保留原材质颜色，强制使用线框 PSO
            for (const auto& entry : m_originalMaterials)
            {
                if (entry.first != nullptr)
                {
                    entry.first->setPipelineOverride(m_wirePipeline);
                }
            }
            break;

        case YalnRenderMode::WhiteAndWire:
            // 白膜填充：用白膜 PSO 覆盖管线；再叠加线框 Pass
            for (const auto& entry : m_originalMaterials)
            {
                if (entry.first != nullptr)
                {
                    entry.first->setPipelineOverride(m_whitePipeline);
                }
            }
            if (m_overlayPass != nullptr)
            {
                m_overlayPass->setPipeline(m_wirePipeline);
            }
            setOverlayEnabled(true);
            break;

        case YalnRenderMode::NormalAndWire:
            // 正常填充：restore 后不覆盖材质/管线（保留顶点色）；
            // 再叠加线框 Pass，得到“材质渲染 + 网格”效果。
            if (m_overlayPass != nullptr)
            {
                m_overlayPass->setPipeline(m_wirePipeline);
            }
            setOverlayEnabled(true);
            break;
    }

    m_mode = mode;
    std::cout << "[RenderMode] 切换到: " << toString(mode)
              << " (whitePSO=" << m_whitePipeline
              << ", wirePSO=" << m_wirePipeline << ")" << std::endl;
    return true;
}
