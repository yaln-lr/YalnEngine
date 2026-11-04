#ifndef YALN_SURFACE_H
#define YALN_SURFACE_H

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "global_type.h"

// ============================================================================
//  YalnSurface
//  职责：仅持有窗口表面（VkSurfaceKHR）。
//        具体创建方式（Win32 / XCB / Metal）由平台层负责，
//        因为只有平台层才知道窗口句柄从哪里来。
// ============================================================================
class YALN_CORE_EXPORT YalnSurface
{
public:
    YalnSurface() = default;

    YalnSurface(const YalnSurface&)            = delete;
    YalnSurface& operator=(const YalnSurface&) = delete;

    // 接管平台层创建好的表面
    void set(vk::raii::SurfaceKHR&& surface)
    {
        m_surface = std::move(surface);
        m_raw     = m_surface != nullptr ? static_cast<VkSurfaceKHR>(*m_surface) : VK_NULL_HANDLE;
    }

    void destroy()
    {
        m_surface = nullptr;
        m_raw     = VK_NULL_HANDLE;
    }

    bool isValid() const { return m_surface != nullptr; }

    const vk::raii::SurfaceKHR& handle() const { return m_surface; }

    // 部分 C API 查询（如 vkGetPhysicalDeviceSurfaceSupportKHR）用裸句柄更稳妥
    VkSurfaceKHR rawHandle() const { return m_raw; }

private:
    vk::raii::SurfaceKHR m_surface{ nullptr };
    VkSurfaceKHR         m_raw = VK_NULL_HANDLE;
};

#endif // YALN_SURFACE_H
