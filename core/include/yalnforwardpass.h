#ifndef YALN_FORWARD_PASS_H
#define YALN_FORWARD_PASS_H

#include "global_type.h"
#include "yalnframe.h"

// ============================================================================
//  YalnForwardOpaquePass —— 默认前向不透明 Pass
//
//  完全通过公开数据工作：
//    scene.drawItems() → 材质库选管线/参数 → 管线库取 PSO → drawIndexed
//  批处理顺序：pipeline → material → mesh（减少状态切换）。
//
//  渲染通道约定：默认 YalnForwardFramePipeline 负责 beginRenderPass /
//  viewport / endRenderPass；本 Pass 只在当前激活的通道内录制绘制命令。
// ============================================================================
class YALN_CORE_EXPORT YalnForwardOpaquePass : public IYalnFramePass
{
public:
    const char* name() const override { return "Forward.Opaque"; }
    uint32_t    passMask() const override { return toPassMask(YalnPassKind::Opaque); }

    void record(const YalnFrameContext& context) override;
};

#endif // YALN_FORWARD_PASS_H
