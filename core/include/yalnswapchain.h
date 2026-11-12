#ifndef YALN_SWAPCHAIN_H
#define YALN_SWAPCHAIN_H

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <vector>

#include "global_type.h"
#include "yalninterfaces.h"

class YalnDevice;
class YalnSurface;

// ============================================================================
//  YalnSwapChain —— IYalnSwapChain 默认实现
//  职责：交换链本体、交换链图像、图像视图、格式与尺寸的选择。
//        不持有帧缓冲（帧缓冲依附渲染通道，见 YalnRenderPass）。
// ============================================================================
class YALN_CORE_EXPORT YalnSwapChain : public IYalnSwapChain
{
public:
    YalnSwapChain() = default;

    YalnSwapChain(const YalnSwapChain&)            = delete;
    YalnSwapChain& operator=(const YalnSwapChain&) = delete;

    bool create(const IYalnDevice& device, const IYalnSurface& surface) override;
    void destroy() override;

    const vk::raii::SwapchainKHR&           handle()     const override { return m_swapChain; }
    const std::vector<vk::Image>&           images()     const override { return m_images; }
    const std::vector<vk::raii::ImageView>& imageViews() const override { return m_imageViews; }
    vk::Format                              format()     const override { return m_format; }
    vk::Extent2D                            extent()     const override { return m_extent; }

    bool isValid() const override { return m_swapChain != nullptr; }

    static vk::SurfaceFormatKHR chooseSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats);
    static vk::PresentModeKHR   choosePresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes);
    vk::Extent2D                chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities) const;

private:
    bool createImageViews();

    const IYalnDevice*  m_device  = nullptr;
    const IYalnSurface* m_surface = nullptr;

    vk::raii::SwapchainKHR           m_swapChain{ nullptr };
    std::vector<vk::Image>           m_images;
    std::vector<vk::raii::ImageView> m_imageViews;
    vk::Format                       m_format{};
    vk::Extent2D                     m_extent{};
};

#endif // YALN_SWAPCHAIN_H
