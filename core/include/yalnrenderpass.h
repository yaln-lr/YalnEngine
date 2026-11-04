#ifndef YALN_RENDERPASS_H
#define YALN_RENDERPASS_H

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <vector>

#include "global_type.h"

class YalnDevice;
class YalnSwapChain;

// ============================================================================
//  YalnRenderPass
//  职责：渲染通道 + 依附于交换链图像视图的帧缓冲。
//        交换链重建时只需重建帧缓冲，渲染通道本身可复用。
// ============================================================================
class YALN_CORE_EXPORT YalnRenderPass
{
public:
    YalnRenderPass() = default;

    YalnRenderPass(const YalnRenderPass&)            = delete;
    YalnRenderPass& operator=(const YalnRenderPass&) = delete;

    bool create(const YalnDevice& device, vk::Format colorFormat);
    bool createFramebuffers(const YalnDevice& device, const YalnSwapChain& swapChain);

    void destroyFramebuffers();
    void destroy();

    const vk::raii::RenderPass&               handle()      const { return m_renderPass; }
    const std::vector<vk::raii::Framebuffer>& framebuffers() const { return m_framebuffers; }

    bool isValid() const { return m_renderPass != nullptr; }

private:
    vk::raii::RenderPass               m_renderPass{ nullptr };
    std::vector<vk::raii::Framebuffer> m_framebuffers;
};

#endif // YALN_RENDERPASS_H
