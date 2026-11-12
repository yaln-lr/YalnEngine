#ifndef YALN_RENDERPASS_H
#define YALN_RENDERPASS_H

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <vector>

#include "global_type.h"
#include "yalninterfaces.h"

class YalnDevice;
class YalnSwapChain;

// ============================================================================
//  YalnRenderPass —— IYalnRenderPass 默认实现
//  职责：按配置创建渲染通道 + 依附于交换链图像视图的帧缓冲。
//        可选深度附件由本类自动分配（图像 + 显存 + 视图，随帧缓冲重建）。
//        交换链重建时只需重建帧缓冲（含深度缓冲），渲染通道本身可复用。
// ============================================================================
class YALN_CORE_EXPORT YalnRenderPass : public IYalnRenderPass
{
public:
    YalnRenderPass() = default;

    YalnRenderPass(const YalnRenderPass&)            = delete;
    YalnRenderPass& operator=(const YalnRenderPass&) = delete;

    // colorFormat：颜色附件格式（一般传 swapChain.format()）
    bool create(const IYalnDevice&    device,
                vk::Format            colorFormat,
                const YalnRenderPassConfig& config = YalnRenderPassConfig::makeDefault()) override;

    bool createFramebuffers(const IYalnDevice& device, const IYalnSwapChain& swapChain) override;

    void destroyFramebuffers() override;
    void destroy() override;

    const vk::raii::RenderPass&               handle()       const override { return m_renderPass; }
    const std::vector<vk::raii::Framebuffer>& framebuffers() const override { return m_framebuffers; }

    bool hasDepth() const override { return m_config.enableDepth; }
    vk::SampleCountFlagBits samples() const override { return m_config.samples; }
    const YalnRenderPassConfig& config() const { return m_config; }

    bool isValid() const override { return m_renderPass != nullptr; }

private:
    static vk::Format findDepthFormat(const IYalnDevice& device);

    YalnRenderPassConfig m_config;
    vk::Format           m_depthFormat = vk::Format::eUndefined;

    vk::raii::RenderPass               m_renderPass{ nullptr };
    std::vector<vk::raii::Framebuffer> m_framebuffers;

    // 可选深度附件（数量 = 交换链图像数量，随帧缓冲一同重建/销毁）
    std::vector<vk::raii::Image>        m_depthImages;
    std::vector<vk::raii::DeviceMemory> m_depthMemories;
    std::vector<vk::raii::ImageView>    m_depthImageViews;
};

using YalnRenderPassPtr = std::shared_ptr<YalnRenderPass>;
#endif // YALN_RENDERPASS_H
