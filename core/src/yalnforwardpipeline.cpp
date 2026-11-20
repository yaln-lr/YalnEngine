#include "../include/yalnforwardpipeline.h"
#include "../include/yalnforwardpass.h"
#include "../include/yalninterfaces.h"

#include <array>
#include <iostream>
#include <utility>

YalnForwardFramePipeline::YalnForwardFramePipeline()
{
    addPass(std::make_unique<YalnForwardOpaquePass>());
}

bool YalnForwardFramePipeline::initialize(const IYalnDevice& device,
                                          const IYalnSwapChain& swapChain)
{
    m_device = &device;
    m_swapChain = &swapChain;

    for (const auto& pass : m_passes)
    {
        if (pass != nullptr && !pass->initialize(device, swapChain))
        {
            std::cerr << "[YalnForwardFramePipeline] Pass 初始化失败: "
                      << pass->name() << std::endl;
            return false;
        }
    }
    return true;
}

void YalnForwardFramePipeline::shutdown()
{
    for (const auto& pass : m_passes)
    {
        if (pass != nullptr)
        {
            pass->shutdown();
        }
    }
    m_device = nullptr;
    m_swapChain = nullptr;
}

bool YalnForwardFramePipeline::recreate(const IYalnSwapChain& swapChain)
{
    m_swapChain = &swapChain;

    for (const auto& pass : m_passes)
    {
        if (pass != nullptr && !pass->recreate(swapChain))
        {
            std::cerr << "[YalnForwardFramePipeline] Pass 重建失败: "
                      << pass->name() << std::endl;
            return false;
        }
    }
    return true;
}

void YalnForwardFramePipeline::record(const YalnFrameContext& context)
{
    if (context.commandBuffer == VK_NULL_HANDLE ||
        context.swapChain == nullptr ||
        context.renderPass == nullptr ||
        context.rendererConfig == nullptr)
    {
        return;
    }

    const auto& framebuffers = context.renderPass->framebuffers();
    if (context.imageIndex >= framebuffers.size())
    {
        return;
    }

    // 默认 Forward 拥有主渲染通道：清屏 + 深度，随后按顺序执行 Pass。
    const vk::Extent2D extent = context.swapChain->extent();
    const vk::ClearColorValue clearColor(context.rendererConfig->clearColor);

    std::array<vk::ClearValue, 2> clearValues = {
        vk::ClearValue(clearColor),
        vk::ClearValue(vk::ClearDepthStencilValue(1.0f, 0))
    };
    const uint32_t clearCount = context.renderPass->hasDepth() ? 2u : 1u;

    const vk::RenderPassBeginInfo renderPassInfo(
        *context.renderPass->handle(),
        *framebuffers[context.imageIndex],
        { { 0, 0 }, extent },
        clearCount,
        clearValues.data());

    context.commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

    const vk::Viewport viewport(0.0f, 0.0f,
                                static_cast<float>(extent.width),
                                static_cast<float>(extent.height),
                                0.0f, 1.0f);
    context.commandBuffer.setViewport(0, viewport);
    context.commandBuffer.setScissor(0, vk::Rect2D({ 0, 0 }, extent));

    for (const auto& pass : m_passes)
    {
        if (pass != nullptr)
        {
            pass->record(context);
        }
    }

    context.commandBuffer.endRenderPass();
}

void YalnForwardFramePipeline::addPass(std::unique_ptr<IYalnFramePass> pass)
{
    if (pass == nullptr)
    {
        return;
    }

    // 已初始化时立即初始化新 Pass，支持运行期追加无资源/可延迟资源的 Pass。
    if (m_device != nullptr && m_swapChain != nullptr)
    {
        if (!pass->initialize(*m_device, *m_swapChain))
        {
            std::cerr << "[YalnForwardFramePipeline] 新增 Pass 初始化失败，已忽略: "
                      << pass->name() << std::endl;
            return;
        }
    }

    m_passes.push_back(std::move(pass));
}
