#include "../include/yalnview.h"

#include <iostream>

YalnView::YalnView() = default;

YalnView::~YalnView()
{
    // 先让设备空闲并释放帧循环相关资源，再交给成员的逆序析构
    m_renderer.shutdown();
    m_device.waitIdle();
}

// ======================== 初始化编排 ========================

bool YalnView::initVulkan(const char* appName)
{
    if (m_initialized)
    {
        return true;
    }

    // 1. 实例 + 调试信使
    if (!createVulkanInstance(appName))
    {
        std::cerr << "Failed to create Vulkan instance" << std::endl;
        return false;
    }

    // 2. 窗口表面（由平台层实现）
    if (!createSurface())
    {
        std::cerr << "Failed to create surface" << std::endl;
        return false;
    }

    // 3. 物理设备 + 逻辑设备 + 队列
    if (!m_device.initialize(m_instance, m_surface))
    {
        return false;
    }

    // 4. 交换链 + 图像视图
    if (!m_swapChain.create(m_device, m_surface))
    {
        std::cerr << "Failed to create swap chain" << std::endl;
        return false;
    }

    // 5. 渲染通道
    if (!m_renderPass.create(m_device, m_swapChain.format()))
    {
        std::cerr << "Failed to create render pass" << std::endl;
        return false;
    }

    // 6. 管线（描述符集布局、管线布局、图形管线）
    if (!m_pipeline.create(m_device, m_renderPass.handle(), m_vertShaderPath, m_fragShaderPath))
    {
        std::cerr << "Failed to create graphics pipeline" << std::endl;
        return false;
    }

    // 7. 帧缓冲（依赖交换链图像视图 + 渲染通道）
    if (!m_renderPass.createFramebuffers(m_device, m_swapChain))
    {
        std::cerr << "Failed to create framebuffers" << std::endl;
        return false;
    }

    // 8. 渲染器：命令池/命令缓冲/同步对象/UBO/描述符
    if (!m_renderer.initialize(m_device, m_swapChain, m_pipeline))
    {
        std::cerr << "Failed to initialize renderer" << std::endl;
        return false;
    }

    m_initialized = true;
    return true;
}

bool YalnView::createVulkanInstance(const char* appName)
{
    return m_instance.create(appName, getRequiredInstanceExtensions());
}

std::vector<const char*> YalnView::getRequiredInstanceExtensions()
{
    std::vector<const char*> requiredExtensions = {
        VK_KHR_SURFACE_EXTENSION_NAME
    };

#ifdef _WIN32
    requiredExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(__linux__)
    requiredExtensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#elif defined(__APPLE__)
    requiredExtensions.push_back(VK_MVK_MACOS_SURFACE_EXTENSION_NAME);
#endif

    if (YalnVulkanInstance::validationLayerEnabled())
    {
        requiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return requiredExtensions;
}

// ======================== 交换链重建 ========================

void YalnView::recreateSwapChain()
{
    if (!m_initialized)
    {
        return;
    }

    m_device.waitIdle();

    m_renderPass.destroyFramebuffers();
    m_swapChain.destroy();

    if (!m_swapChain.create(m_device, m_surface))
    {
        std::cerr << "Failed to recreate swap chain" << std::endl;
        return;
    }

    if (!m_renderPass.createFramebuffers(m_device, m_swapChain))
    {
        std::cerr << "Failed to recreate framebuffers" << std::endl;
        return;
    }

    // 图像数量可能变化，命令缓冲/描述符/同步对象需要一起重建
    m_renderer.recreate(m_swapChain, m_pipeline);
}

// ======================== 帧循环 ========================

void YalnView::drawFrame()
{
    if (!m_initialized)
    {
        return;
    }

    const auto result = m_renderer.drawFrame(m_swapChain, m_pipeline, m_renderPass, m_scene, m_camera_ptr);
    if (result == YalnRenderer::FrameResult::SwapChainOutOfDate)
    {
        recreateSwapChain();
    }
}

// ======================== 场景 ========================

void YalnView::addMesh(YalnMesh* mesh)
{
    m_scene.addMesh(mesh);

    if (m_initialized && mesh != nullptr && mesh->isValid())
    {
        createMeshBuffers();
    }
}

void YalnView::createMeshBuffers()
{
    m_device.waitIdle();
    m_scene.buildGpuBuffers(m_device, m_renderer.commandPool());
}

void YalnView::clearMeshes()
{
    m_device.waitIdle();
    m_scene.clear();
}

bool YalnView::isMeshReady() const
{
    return m_scene.isReady();
}

// ======================== 其它 ========================

void YalnView::setCameta(YalnCameraPtr camera)
{
    m_camera_ptr = std::move(camera);
}

void YalnView::printCapacity()
{
    m_device.printCapacity();
}
