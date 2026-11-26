#ifndef YALN_FRAME_H
#define YALN_FRAME_H

#include <vulkan/vulkan.hpp>

#include <cstdint>
#include <string_view>

#include "global_type.h"
#include "yalninterfaces.h"
#include "yalnpipelinelibrary.h"
#include "yalnmateriallibrary.h"

// ============================================================================
//  帧级渲染策略（Frame Pipeline / Frame Pass）
//
//  与 IYalnPipeline（Vulkan PSO）严格区分：
//    IYalnFramePipeline —— 整帧怎么组织（前向 / 延迟 / 后处理链）
//    IYalnFramePass     —— 整帧中的一个逻辑阶段（Shadow / Opaque / ...）
//    IYalnPipeline      —— 某个 Pass 内实际绑定的 Vulkan PSO
//
//  默认 Forward 策略只使用公开接口；自定义策略可实现 IYalnFramePipeline，
//  通过 YalnView::setFramePipeline() 注入，YalnRenderer 本身保持稳定。
// ============================================================================

// 每帧录制时传给 FramePipeline / FramePass 的只读上下文。
// 所有指针在 record() 期间有效，实现不得保存。
struct YALN_CORE_EXPORT YalnFrameContext
{
    vk::CommandBuffer commandBuffer = VK_NULL_HANDLE;
    uint32_t          imageIndex = 0;

    const IYalnSwapChain*  swapChain  = nullptr;
    const IYalnRenderPass* renderPass = nullptr;
    const IYalnScene*      scene      = nullptr;
    const YalnCamera*      camera     = nullptr;

    const YalnRendererConfig* rendererConfig = nullptr;
    const YalnPipelineLibrary* pipelines     = nullptr;
    const YalnMaterialLibrary* materials     = nullptr;

    // 渲染器为当前交换链图像分配好的 UBO 描述符集；无描述符布局时为空。
    vk::DescriptorSet frameDescriptorSet = VK_NULL_HANDLE;
};

// 一个逻辑 Pass。默认 Forward 的 Opaque Pass 是它的一个实现。
// 默认 Forward 中由 FramePipeline 负责主渲染通道的 begin/end，
// 因此 Pass 只在当前激活的通道内录制绘制命令；自定义 FramePipeline
// 可以定义自己的渲染通道契约。
class YALN_CORE_EXPORT IYalnFramePass
{
public:
    virtual ~IYalnFramePass() = default;

    virtual std::string_view name() const = 0;
    virtual YalnPassMask     passMask() const = 0;

    // 交换链创建/重建时调用；无资源的 Pass 可保持默认实现。
    virtual bool initialize([[maybe_unused]] const IYalnDevice& device,
                            [[maybe_unused]] const IYalnSwapChain& swapChain)
    {
        return true;
    }

    virtual void shutdown() {}

    virtual bool recreate([[maybe_unused]] const IYalnSwapChain& swapChain)
    {
        return true;
    }

    // 录制本 Pass 的全部命令（是否 begin/end renderPass 由实现决定）
    virtual void record(const YalnFrameContext& context) = 0;
};

// 整帧策略。一个 View 一个 FramePipeline；内部可包含多个 FramePass。
class YALN_CORE_EXPORT IYalnFramePipeline
{
public:
    virtual ~IYalnFramePipeline() = default;

    virtual std::string_view name() const = 0;

    virtual bool initialize([[maybe_unused]] const IYalnDevice& device,
                            [[maybe_unused]] const IYalnSwapChain& swapChain)
    {
        return true;
    }

    virtual void shutdown() {}

    virtual bool recreate([[maybe_unused]] const IYalnSwapChain& swapChain)
    {
        return true;
    }

    virtual void record(const YalnFrameContext& context) = 0;
};

#endif // YALN_FRAME_H
