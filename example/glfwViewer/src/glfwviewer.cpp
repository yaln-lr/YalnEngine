#include "../include/glfwviewer.h"
#include "yalnperspectivecamera.h"
#include "YalnMesh.h"

#include <iostream>

GlfwViewer::GlfwViewer(int width, int height, const char* title)
    : _window(nullptr)
    , _width(width)
    , _height(height)
    , _title(title)
{
}

GlfwViewer::~GlfwViewer()
{
    cleanup();

    if (_window)
    {
        glfwDestroyWindow(_window);
        _window = nullptr;
    }
    glfwTerminate();
}

bool GlfwViewer::initWindow()
{
    // 初始化 GLFW
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    // 配置 GLFW：不使用默认 OpenGL 上下文
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    // 支持窗口调整大小
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    // 创建窗口
    _window = glfwCreateWindow(_width, _height, _title, nullptr, nullptr);
    if (!_window)
    {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    // 设置用户指针和尺寸改变回调
    glfwSetWindowUserPointer(_window, this);
    glfwSetFramebufferSizeCallback(_window, framebufferResizeCallback);

    return true;
}

bool GlfwViewer::createSurface()
{
    // 使用 GLFW 创建 Vulkan 表面
    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(*m_instance, _window, nullptr, &surface) != VK_SUCCESS)
    {
        std::cerr << "Failed to create window surface" << std::endl;
        return false;
    }

    // 使用 vk::raii::SurfaceKHR 包装原始句柄
    m_surface = vk::raii::SurfaceKHR(m_instance, surface);
    m_surfaceHandle = surface;

    return true;
}

std::vector<const char*> GlfwViewer::getRequiredInstanceExtensions()
{
    // 获取 GLFW 需要的扩展
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    // 添加调试工具扩展
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    return extensions;
}

void GlfwViewer::framebufferResizeCallback(GLFWwindow* window, int /*width*/, int /*height*/)
{
    auto* viewer = static_cast<GlfwViewer*>(glfwGetWindowUserPointer(window));
    if (viewer)
    {
        viewer->m_framebufferResized = true;
    }
}

void GlfwViewer::recreateSwapChain()
{
    // 处理最小化窗口
    int width = 0, height = 0;
    glfwGetFramebufferSize(_window, &width, &height);
    while (width == 0 || height == 0)
    {
        glfwGetFramebufferSize(_window, &width, &height);
        glfwWaitEvents();
    }

    YalnView::recreateSwapChain();

    float aspect = static_cast<float>(m_swapChainExtent.width) / static_cast<float>(m_swapChainExtent.height);
    m_camera_ptr->setAspectRatio(aspect);
 
}

void GlfwViewer::mainLoop()
{
    while (!glfwWindowShouldClose(_window))
    {
        glfwPollEvents();

        // 检查窗口尺寸是否改变
        if (m_framebufferResized)
        {
            recreateSwapChain();
        }

        // 绘制帧
        drawFrame();
    }

    // 等待设备空闲后再继续清理
    m_device.waitIdle();
}

void GlfwViewer::run()
{
    // 1. 初始化窗口
    if (!initWindow())
    {
        return;
    }

    if (!initVulkan(_title))
    {
        return;
    }

    float aspect = static_cast<float>(m_swapChainExtent.width) / static_cast<float>(m_swapChainExtent.height);
    // 设置相机
    setCameta(YalnPerspectiveCameraPtr(new YalnPerspectiveCamera(45.0,aspect,1.0,10.0)));
    
    // 设置网格（延迟创建顶点/索引缓冲区）
    YalnCube cube(1.0f, 1.0f, 1.0f);
    YalnSphere aphere;
    setMesh(&aphere);
    
    // 进入主循环
    mainLoop();
}
