#include "../include/yalnrenderpass.h"
#include "../include/yalndevice.h"
#include "../include/yalnswapchain.h"

#include <array>
#include <iostream>

namespace {
// 常用深度格式候选（按精度优先）
constexpr std::array<vk::Format, 3> kDepthFormatCandidates = {
    vk::Format::eD32Sfloat,
    vk::Format::eD24UnormS8Uint,
    vk::Format::eD32SfloatS8Uint
};
}

vk::Format YalnRenderPass::findDepthFormat(const IYalnDevice& device)
{
    for (const vk::Format format : kDepthFormatCandidates)
    {
        const vk::FormatProperties props = device.physicalDevice().getFormatProperties(format);
        if (props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment)
        {
            return format;
        }
    }
    return vk::Format::eUndefined;
}

bool YalnRenderPass::create(const IYalnDevice& device,
                            vk::Format        colorFormat,
                            const YalnRenderPassConfig& config)
{
    destroy();
    m_config = config;

    // ---- 附件列表：[0]=颜色，[1]=深度（可选）----
    std::vector<vk::AttachmentDescription> attachments;
    attachments.push_back(vk::AttachmentDescription(
        {},
        colorFormat,
        config.samples,
        config.colorLoadOp,
        config.colorStoreOp,
        vk::AttachmentLoadOp::eDontCare,
        vk::AttachmentStoreOp::eDontCare,
        vk::ImageLayout::eUndefined,
        config.colorFinalLayout));

    vk::AttachmentReference colorAttachmentRef(0, vk::ImageLayout::eColorAttachmentOptimal);
    std::optional<vk::AttachmentReference> depthAttachmentRef;

    if (config.enableDepth)
    {
        m_depthFormat = findDepthFormat(device);
        if (m_depthFormat == vk::Format::eUndefined)
        {
            std::cerr << "[YalnRenderPass] 设备不支持任何深度格式，无法启用深度附件" << std::endl;
            return false;
        }

        attachments.push_back(vk::AttachmentDescription(
            {},
            m_depthFormat,
            config.samples,
            config.depthLoadOp,
            config.depthStoreOp,
            vk::AttachmentLoadOp::eDontCare,
            vk::AttachmentStoreOp::eDontCare,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eDepthStencilAttachmentOptimal));

        depthAttachmentRef = vk::AttachmentReference(1, vk::ImageLayout::eDepthStencilAttachmentOptimal);
    }

    // ---- 子通道：颜色（+ 深度）----
    vk::SubpassDescription subpass(
        {},
        vk::PipelineBindPoint::eGraphics,
        0, nullptr,
        1, &colorAttachmentRef,
        nullptr,
        depthAttachmentRef ? &*depthAttachmentRef : nullptr);

    // ---- 依赖：外部 -> 子通道 ----
    std::vector<vk::SubpassDependency> dependencies;
    dependencies.push_back(vk::SubpassDependency(
        VK_SUBPASS_EXTERNAL, 0,
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        {},
        vk::AccessFlagBits::eColorAttachmentWrite));

    if (config.enableDepth)
    {
        dependencies.push_back(vk::SubpassDependency(
            VK_SUBPASS_EXTERNAL, 0,
            vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests,
            vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests,
            {},
            vk::AccessFlagBits::eDepthStencilAttachmentWrite));
    }

    vk::RenderPassCreateInfo renderPassInfo(
        {},
        attachments,
        subpass,
        dependencies);

    m_renderPass = vk::raii::RenderPass(device.device(), renderPassInfo);
    return m_renderPass != nullptr;
}

bool YalnRenderPass::createFramebuffers(const IYalnDevice& device, const IYalnSwapChain& swapChain)
{
    destroyFramebuffers();

    if (!isValid() || !swapChain.isValid())
    {
        return false;
    }

    const vk::Extent2D extent = swapChain.extent();
    const size_t imageCount = swapChain.imageViews().size();

    // ---- 可选：为每张交换链图像分配深度缓冲 ----
    if (m_config.enableDepth)
    {
        m_depthImages.reserve(imageCount);
        m_depthMemories.reserve(imageCount);
        m_depthImageViews.reserve(imageCount);

        for (size_t i = 0; i < imageCount; i++)
        {
            vk::ImageCreateInfo imageInfo(
                {},
                vk::ImageType::e2D,
                m_depthFormat,
                vk::Extent3D(extent.width, extent.height, 1),
                1, 1,
                m_config.samples,
                vk::ImageTiling::eOptimal,
                vk::ImageUsageFlagBits::eDepthStencilAttachment,
                vk::SharingMode::eExclusive,
                {},
                vk::ImageLayout::eUndefined);

            m_depthImages.emplace_back(device.device(), imageInfo);

            const vk::MemoryRequirements requirements = m_depthImages.back().getMemoryRequirements();
            vk::MemoryAllocateInfo allocateInfo(
                requirements.size,
                device.findMemoryType(requirements.memoryTypeBits,
                                      vk::MemoryPropertyFlagBits::eDeviceLocal));

            m_depthMemories.emplace_back(device.device(), allocateInfo);
            static_cast<vk::Device>(device.device()).bindImageMemory(
                *m_depthImages.back(), *m_depthMemories.back(), 0);

            vk::ImageViewCreateInfo viewInfo(
                {},
                *m_depthImages.back(),
                vk::ImageViewType::e2D,
                m_depthFormat,
                {},
                vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1));

            m_depthImageViews.emplace_back(device.device(), viewInfo);
        }
    }

    // ---- 帧缓冲：颜色视图（+ 深度视图）----
    m_framebuffers.reserve(imageCount);
    for (size_t i = 0; i < imageCount; i++)
    {
        std::vector<vk::ImageView> attachments;
        attachments.push_back(*swapChain.imageViews()[i]);
        if (m_config.enableDepth)
        {
            attachments.push_back(*m_depthImageViews[i]);
        }

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
    m_depthImageViews.clear();
    m_depthMemories.clear();
    m_depthImages.clear();
}

void YalnRenderPass::destroy()
{
    destroyFramebuffers();
    m_renderPass   = nullptr;
    m_depthFormat  = vk::Format::eUndefined;
}
