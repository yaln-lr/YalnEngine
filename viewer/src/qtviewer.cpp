#include "../include/qtviewer.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <iostream>
#include <glm/gtc/matrix_transform.hpp>

QtViewer::QtViewer(QWidget* parent)
    : QWidget(parent)
    , YalnView()
    , m_autoRotate(true)
    , m_autoRotateSpeed(1.0f)
    , m_isMousePressed(false)
    , m_lastMouseX(0.0f)
    , m_lastMouseY(0.0f)
    , m_mouseButton(-1)
    , m_rotationSensitivity(0.005f)
    , m_panSensitivity(0.005f)
    , m_zoomSensitivity(0.002f)
    , m_frameCount(0)
    , m_width(800)
    , m_height(600)
{
    // 设置 Qt Widget 属性
    setMinimumSize(640, 480);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    // 启用原生窗口
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_NativeWindow);
}

QtViewer::~QtViewer()
{
    // 停止渲染定时器
    if (m_renderTimer) {
        m_renderTimer->stop();
        delete m_renderTimer;
        m_renderTimer = nullptr;
    }
}

bool QtViewer::createSurface()
{
#ifdef _WIN32
    // 使用 Vulkan HPP 接口创建 Win32 表面
    vk::Win32SurfaceCreateInfoKHR createInfo(
        vk::Win32SurfaceCreateFlagsKHR{},  // flags
        GetModuleHandle(nullptr),          // hinstance
        (HWND)winId()                       // hwnd
    );

    try {
        // 使用 vk::raii 方式创建表面
        m_surface = m_instance.createWin32SurfaceKHR(createInfo);
        
        // 获取原始句柄用于某些查询
        VkSurfaceKHR rawSurface = static_cast<VkSurfaceKHR>(*m_surface);
        m_surfaceHandle = rawSurface;
        return true;
    }
    catch (const vk::SystemError& e) {
        std::cerr << "Failed to create Win32 surface: " << e.what() << std::endl;
        return false;
    }

#elif defined(__linux__)
    std::cerr << "Linux XCB surface creation not implemented" << std::endl;
    return false;

#elif defined(__APPLE__)
    vk::MacOSSurfaceCreateInfoMVK createInfo(
        vk::MacOSSurfaceCreateFlagsMVK{},
        (void*)winId()
    );

    try {
        m_surface = m_instance.createMacOSSurfaceMVK(createInfo);
        m_surfaceHandle = static_cast<VkSurfaceKHR>(*m_surface);
        return true;
    }
    catch (const vk::SystemError& e) {
        std::cerr << "Failed to create macOS surface: " << e.what() << std::endl;
        return false;
    }

#else
    std::cerr << "Unsupported platform for surface creation" << std::endl;
    return false;
#endif
}

std::vector<const char*> QtViewer::getRequiredInstanceExtensions()
{
    // 调用基类方法获取验证层等扩展
    auto extensions = YalnView::getRequiredInstanceExtensions();

#ifdef _WIN32
    extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(__linux__)
    extensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#elif defined(__APPLE__)
    extensions.push_back(VK_MVK_MACOS_SURFACE_EXTENSION_NAME);
#endif

    return extensions;
}

bool QtViewer::initialize(const char* appName)
{
    // 调用基类初始化
    if (!initVulkan(appName)) {
        emit vulkanInitialized(false);
        return false;
    }

    // 更新窗口尺寸
    QSize size = this->size();
    m_width = size.width() > 0 ? size.width() : 800;
    m_height = size.height() > 0 ? size.height() : 600;

    emit vulkanInitialized(true);
    return true;
}

void QtViewer::startRenderLoop()
{
    if (!m_renderTimer) {
        m_renderTimer = new QTimer(this);
        connect(m_renderTimer, &QTimer::timeout, this, &QtViewer::onFrameTimeout);
    }
    m_renderTimer->start(16); // ~60 FPS
}

void QtViewer::onFrameTimeout()
{
    // 自动旋转
    if (m_autoRotate && m_camera_ptr) {
        // 相机自动旋转
        glm::vec3 pos = m_camera_ptr->getPosition();
        float angle = m_autoRotateSpeed * 0.016f;
        
        // 绕 Y 轴旋转
        float cosA = glm::cos(angle);
        float sinA = glm::sin(angle);
        float x = pos.x * cosA - pos.z * sinA;
        float z = pos.x * sinA + pos.z * cosA;
        pos = glm::vec3(x, pos.y, z);
        
        m_camera_ptr->setPosition(pos);
    }

    // 绘制帧
    drawFrame();
    m_frameCount++;

    emit frameRendered(m_frameCount);
}

void QtViewer::paintEvent(QPaintEvent* event)
{
    (void)event;
    // Vulkan 渲染由定时器控制，此处不需要额外处理
}

void QtViewer::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    // 更新尺寸
    QSize size = event->size();
    m_width = size.width();
    m_height = size.height();
    std::cerr << "[RESIZE] Qt size: " << m_width << "x" << m_height << std::endl;

    // 如果 Vulkan 已初始化，重新创建交换链
    if (m_swapChain != nullptr) {
        recreateSwapChain();
        // 使用交换链的实际大小更新相机宽高比
        if (m_camera_ptr) {
            std::cerr << "[RESIZE] SwapChain extent: " << m_swapChainExtent.width << "x" << m_swapChainExtent.height << std::endl;
            float aspect = static_cast<float>(m_swapChainExtent.width) / static_cast<float>(m_swapChainExtent.height);
            m_camera_ptr->setAspectRatio(aspect);
            std::cerr << "[RESIZE] Camera aspect ratio set to: " << aspect << std::endl;
        }
    }
}

void QtViewer::keyPressEvent(QKeyEvent* event)
{
    handleKeyInput(event->key(), true);
    event->accept();
}

void QtViewer::keyReleaseEvent(QKeyEvent* event)
{
    handleKeyInput(event->key(), false);
    event->accept();
}

void QtViewer::handleKeyInput(int key, bool pressed)
{
    if (!pressed || !m_camera_ptr) return;

    const float moveSpeed = 0.2f;
    glm::vec3 pos = m_camera_ptr->getPosition();

    switch (key) {
        // WASD：相机位置移动
        case Qt::Key_W:
            pos.z -= moveSpeed;
            break;
        case Qt::Key_S:
            pos.z += moveSpeed;
            break;
        case Qt::Key_A:
            pos.x -= moveSpeed;
            break;
        case Qt::Key_D:
            pos.x += moveSpeed;
            break;
        case Qt::Key_Q:
            pos.y -= moveSpeed;
            break;
        case Qt::Key_E:
            pos.y += moveSpeed;
            break;

        // +/-：调整相机距离
        case Qt::Key_Plus:
        case Qt::Key_Equal: {
            glm::vec3 dir = glm::normalize(-pos);
            pos += dir * 0.5f;
            break;
        }
        case Qt::Key_Minus: {
            glm::vec3 dir = glm::normalize(-pos);
            pos -= dir * 0.5f;
            break;
        }

        // R：重置所有变换
        case Qt::Key_R:
            resetTransform();
            return;

        // Space：切换自动旋转
        case Qt::Key_Space:
            m_autoRotate = !m_autoRotate;
            return;
    }

    m_camera_ptr->setPosition(pos);
}

void QtViewer::mousePressEvent(QMouseEvent* event)
{
    m_isMousePressed = true;
    m_mouseButton = event->button();
    m_lastMouseX = static_cast<float>(event->position().x());
    m_lastMouseY = static_cast<float>(event->position().y());
    event->accept();
}

void QtViewer::mouseReleaseEvent(QMouseEvent* event)
{
    (void)event;
    m_isMousePressed = false;
    m_mouseButton = -1;
    event->accept();
}

void QtViewer::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_isMousePressed || !m_camera_ptr) return;

    float currentX = static_cast<float>(event->position().x());
    float currentY = static_cast<float>(event->position().y());
    float deltaX = currentX - m_lastMouseX;
    float deltaY = currentY - m_lastMouseY;

    glm::vec3 pos = m_camera_ptr->getPosition();
    glm::vec3 target = m_camera_ptr->getTarget();

    if (m_mouseButton == Qt::LeftButton) {
        // 左键：旋转相机视角（绕目标点）
        float pitch = deltaY * m_rotationSensitivity;
        float yaw = deltaX * m_rotationSensitivity;

        // 计算当前角度
        glm::vec3 offset = pos - target;
        float radius = glm::length(offset);
        
        // 计算新的方位角和仰角
        float phi = glm::atan(offset.z, offset.x) + yaw;
        float theta = glm::acos(offset.y / radius) - pitch;
        
        // 限制仰角
        theta = glm::clamp(theta, 0.1f, glm::pi<float>() - 0.1f);
        
        // 计算新位置
        offset.x = radius * glm::sin(theta) * glm::cos(phi);
        offset.y = radius * glm::cos(theta);
        offset.z = radius * glm::sin(theta) * glm::sin(phi);
        
        pos = target + offset;
        m_camera_ptr->setPosition(pos);

    } else if (m_mouseButton == Qt::RightButton) {
        // 右键：平移相机（沿视平面移动）
        glm::vec3 forward = glm::normalize(target - pos);
        glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
        glm::vec3 up = glm::cross(right, forward);

        pos -= right * deltaX * m_panSensitivity * 10.0f;
        pos += up * deltaY * m_panSensitivity * 10.0f;
        target -= right * deltaX * m_panSensitivity * 10.0f;
        target += up * deltaY * m_panSensitivity * 10.0f;

        m_camera_ptr->setPosition(pos);
        m_camera_ptr->setTarget(target);
    }

    m_lastMouseX = currentX;
    m_lastMouseY = currentY;
    event->accept();
}

void QtViewer::wheelEvent(QWheelEvent* event)
{
    if (!m_camera_ptr) {
        event->accept();
        return;
    }

    // 滚轮缩放（沿视线方向移动相机）
    float delta = static_cast<float>(event->angleDelta().y());
    glm::vec3 pos = m_camera_ptr->getPosition();
    glm::vec3 target = m_camera_ptr->getTarget();
    glm::vec3 dir = glm::normalize(target - pos);
    
    pos += dir * delta * m_zoomSensitivity;
    m_camera_ptr->setPosition(pos);
    
    event->accept();
}

void QtViewer::resetTransform()
{
    if (m_camera_ptr) {
        m_camera_ptr->setPosition(glm::vec3(2.0f, 2.0f, 2.0f));
        m_camera_ptr->setTarget(glm::vec3(0.0f, 0.0f, 0.0f));
    }
    m_autoRotate = true;
}
