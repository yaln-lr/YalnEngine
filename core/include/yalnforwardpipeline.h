#ifndef YALN_FORWARD_PIPELINE_H
#define YALN_FORWARD_PIPELINE_H

#include <cstddef>
#include <memory>
#include <vector>

#include "global_type.h"
#include "yalnframe.h"

// ============================================================================
//  YalnForwardFramePipeline —— 默认 Forward 整帧策略
//
//  默认包含两个逻辑 Pass：
//    - YalnForwardOpaquePass              主材质（item.material）
//    - YalnMaterialOverlayPass(Outline)   叠加材质（item.overlayMaterials[]）
//  FramePipeline 负责主渲染通道的 begin/end、清屏与 viewport/scissor。
//  可通过 addPass() 追加更多阶段的叠加 Pass（例如 Transparent），
//  或整体替换为自定义策略；需要独立渲染目标的 Pass（如 Shadow）应使用
//  自定义 IYalnFramePipeline。
// ============================================================================
class YALN_CORE_EXPORT YalnForwardFramePipeline : public IYalnFramePipeline
{
public:
    YalnForwardFramePipeline();
    ~YalnForwardFramePipeline() override = default;

    const char* name() const override { return "Forward"; }

    bool initialize(const IYalnDevice& device, const IYalnSwapChain& swapChain) override;
    void shutdown() override;
    bool recreate(const IYalnSwapChain& swapChain) override;

    void record(const YalnFrameContext& context) override;

    // 追加逻辑 Pass（接管所有权）；nullptr 被忽略。
    void addPass(std::unique_ptr<IYalnFramePass> pass);

    const std::vector<std::unique_ptr<IYalnFramePass>>& passes() const { return m_passes; }

private:
    std::vector<std::unique_ptr<IYalnFramePass>> m_passes;
    const IYalnDevice*    m_device = nullptr;
    const IYalnSwapChain* m_swapChain = nullptr;
};

#endif // YALN_FORWARD_PIPELINE_H
