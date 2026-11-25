#ifndef YALN_FORWARD_PASS_H
#define YALN_FORWARD_PASS_H

#include "global_type.h"
#include "yalnframe.h"
#include "yalnrendertypes.h"

// ============================================================================
//  YalnForwardOpaquePass —— 默认前向不透明 Pass
//
//  完全通过公开数据工作：
//    drawItems → 主材质 → 材质引用管线 → PSO → drawIndexed
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

// ============================================================================
//  YalnMaterialOverlayPass —— 通用叠加材质 Pass（方案 A 的第二个逻辑 Pass）
//
//  与 OpaquePass 共享同一批处理实现，区别只在数据来源：
//    - OpaquePass       只画 item.material（主材质）
//    - OverlayPass      画 item.overlayMaterials[]（叠加材质）
//
//  Pass 由构造时指定的 YalnPassKind 决定自己在哪个阶段绘制（如 Outline）。
//  材质自身的 passMask 必须包含该 Pass 位才会被绘制；叠加材质无效时跳过，
//  不回退默认材质（避免意外重画一遍）。
//
//  Pass 常驻在 Forward 的 Pass 列表中，没有 enable 开关：没有任何 drawable
//  携带对应阶段的叠加材质时，它遍历到的批次为空，不录制任何绘制命令。
// ============================================================================
class YALN_CORE_EXPORT YalnMaterialOverlayPass : public IYalnFramePass
{
public:
    explicit YalnMaterialOverlayPass(YalnPassKind pass) : m_pass(pass) {}

    const char* name() const override { return "Forward.MaterialOverlay"; }
    uint32_t    passMask() const override { return toPassMask(m_pass); }

    YalnPassKind pass() const { return m_pass; }
    void setPass(YalnPassKind pass) { m_pass = pass; }

    void record(const YalnFrameContext& context) override;

private:
    YalnPassKind m_pass;
};

#endif // YALN_FORWARD_PASS_H
