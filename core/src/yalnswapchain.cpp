#include "../include/yalnswapchain.h"
#include "../include/yalnsurface.h"
#include "../include/yalndevice.h"

#include <array>
#include <limits>

bool YalnSwapChain::create(const IYalnDevice& device, const IYalnSurface& surface)
{
    m_device  = &device;
    m_surface = &surface;

    const auto support = device.querySwapChainSupport(device.physicalDevice());
    if (support.formats.empty() || support.presentModes.empty())
    {
        return false;
    }

    const vk::SurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(support.formats);
    const vk::PresentModeKHR   presentMode   = choosePresentMode(support.presentModes);
    const vk::Extent2D         extent        = chooseSwapExtent(support.capabilities);

    uint32_t imageCount = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0 && imageCount > support.capabilities.maxImageCount)
    {
        imageCount = support.capabilities.maxImageCount;
    }

    const auto indices = device.queueFamilies();
    const std::array<uint32_t, 2> queueFamilyIndices = {
        indices.graphicsFamily.value(),
        indices.presentFamily.value()
    };

    vk::SwapchainCreateInfoKHR createInfo(
        {},
        *m_surface->handle(),
        imageCount,
        surfaceFormat.format,
        surfaceFormat.colorSpace,
        extent,
        1,
        vk::ImageUsageFlagBits::eColorAttachment,
        vk::SharingMode::eExclusive,
        0, nullptr,
        support.capabilities.currentTransform,
        vk::CompositeAlphaFlagBitsKHR::eOpaque,
        presentMode,
        VK_TRUE,
        nullptr);

    if (indices.graphicsFamily != indices.presentFamily)
    {
        createInfo.imageSharingMode      = vk::SharingMode::eConcurrent;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices   = queueFamilyIndices.data();
    }

    m_swapChain = vk::raii::SwapchainKHR(device.device(), createInfo);
    m_images    = m_swapChain.getImages();
    m_format    = surfaceFormat.format;
    m_extent    = extent;

    return createImageViews();
}

void YalnSwapChain::destroy()
{
    m_imageViews.clear();
    m_images.clear();
    m_swapChain = nullptr;
    m_format    = vk::Format{};
    m_extent    = vk::Extent2D{};
}

bool YalnSwapChain::createImageViews()
{
    m_imageViews.clear();
    m_imageViews.reserve(m_images.size());

    for (const auto& image : m_images)
    {
        vk::ImageViewCreateInfo createInfo(
            {},
            image,
            vk::ImageViewType::e2D,
            m_format,
            {},
            { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });

        m_imageViews.emplace_back(m_device->device(), createInfo);
    }

    return !m_imageViews.empty();
}

vk::SurfaceFormatKHR YalnSwapChain::chooseSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats)
{
    for (const auto& availableFormat : availableFormats)
    {
        if (availableFormat.format == vk::Format::eB8G8R8A8Srgb &&
            availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
        {
            return availableFormat;
        }
    }
    return availableFormats[0];
}

vk::PresentModeKHR YalnSwapChain::choosePresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes)
{
    for (const auto& availablePresentMode : availablePresentModes)
    {
        if (availablePresentMode == vk::PresentModeKHR::eMailbox)
        {
            return availablePresentMode;
        }
    }
    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D YalnSwapChain::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities) const
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return capabilities.currentExtent;
    }
    return capabilities.minImageExtent;
}
