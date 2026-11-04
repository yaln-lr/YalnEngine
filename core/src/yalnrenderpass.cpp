#include "../include/yalnrenderpass.h"
#include "../include/yalndevice.h"
#include "../include/yalnswapchain.h"

#include <array>

bool YalnRenderPass::create(const YalnDevice& device, vk::Format colorFormat)
{
    vk::AttachmentDescription colorAttachment(
        {},
        colorFormat,
        vk::SampleCountFlagBits::e1,
        vk::AttachmentLoadOp::eClear,
        vk::AttachmentStoreOp::eStore,
        vk::AttachmentLoadOp::eDontCare,
        vk::AttachmentStoreOp::eDontCare,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::ePresentSrcKHR);

    vk::AttachmentReference colorAttachmentRef(0, vk::ImageLayout::eColorAttachmentOptimal);

    vk::SubpassDescription subpass(
        {},
        vk::PipelineBindPoint::eGraphics,
        0, nullptr,
        1, &colorAttachmentRef);

    vk::SubpassDependency dependency(
        VK_SUBPASS_EXTERNAL, 0,
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        {},
        vk::AccessFlagBits::eColorAttachmentWrite);

    vk::RenderPassCreateInfo renderPassInfo(
        {},
        1, &colorAttachment,
        1, &subpass,
        1, &dependency);

    m_renderPass = vk::raii::RenderPass(device.device(), renderPassInfo);
    return m_renderPass != nullptr;
}

bool YalnRenderPass::createFramebuffers(const YalnDevice& device, const YalnSwapChain& swapChain)
{
    destroyFramebuffers();

    if (!isValid() || !swapChain.isValid())
    {
        return false;
    }

    const vk::Extent2D extent = swapChain.extent();

    m_framebuffers.reserve(swapChain.imageViews().size());
    for (const auto& imageView : swapChain.imageViews())
    {
        const std::array<vk::ImageView, 1> attachments = { *imageView };

        vk::FramebufferCreateInfo framebufferInfo(
            {},
            *m_renderPass,
            static_cast<uint32_t>(attachments.size()),
            attachments.data(),
            extent.width,
            extent.height,
            1);

        m_framebuffers.emplace_back(device.device(), framebufferInfo);
    }

    return !m_framebuffers.empty();
}

void YalnRenderPass::destroyFramebuffers()
{
    m_framebuffers.clear();
}

void YalnRenderPass::destroy()
{
    m_framebuffers.clear();
    m_renderPass = nullptr;
}
