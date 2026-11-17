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

// 平台层职责：只负责创建表面并交给 YalnSurface，其余交给 YalnView 编排
bool QtViewer::createSurface()
{
#ifdef _WIN32
    vk::Win32SurfaceCreateInfoKHR createInfo(
        vk::Win32SurfaceCreateFlagsKHR{},  // flags
        GetModuleHandle(nullptr),          // hinstance
        (HWND)winId()                      // hwnd
    );

    try {
        auto vulkanSurface = vulkanInstance().handle().createWin32SurfaceKHR(createInfo);
        surface().set(std::move(vulkanSurface));
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
        auto vulkanSurface = vulkanInstance().handle().createMacOSSurfaceMVK(createInfo);
        surface().set(std::move(vulkanSurface));
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

bool QtViewer::initialize(const char* appName)
{
    // 调用基类初始化（内部按顺序驱动各子系统）
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
    // 自动旋转：绕竖直轴 Z 水平公转（引擎 Z 轴向上）
    if (m_autoRotate && camera()) {
        YalnCameraPtr cam = camera();
        glm::vec3 pos = cam->getPosition();
        float angle = m_autoRotateSpeed * 0.016f;

        float cosA = glm::cos(angle);
        float sinA = glm::sin(angle);
        float x = pos.x * cosA - pos.y * sinA;
        float y = pos.x * sinA + pos.y * cosA;
        pos = glm::vec3(x, y, pos.z);   // 高度 z 不变

        cam->setPosition(pos);
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

    // 如果 Vulkan 已初始化，重新创建交换链
    if (isInitialized()) {
        recreateSwapChain();

        // 使用交换链的实际大小更新相机宽高比
        if (camera()) {
            const vk::Extent2D extent = swapChainExtent();
            if (extent.height != 0) {
                float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
                camera()->setAspectRatio(aspect);
            }
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
    if (!pressed || !camera()) return;

    const float moveSpeed = 0.2f;
    glm::vec3 pos = camera()->getPosition();

    switch (key) {
        // WASD：水平移动（Z 轴向上，地面为 XY 平面）
        case Qt::Key_W:
            pos.y -= moveSpeed;   // 前进（-Y）
            break;
        case Qt::Key_S:
            pos.y += moveSpeed;   // 后退（+Y）
            break;
        case Qt::Key_A:
            pos.x -= moveSpeed;
            break;
        case Qt::Key_D:
            pos.x += moveSpeed;
            break;
        case Qt::Key_Q:
            pos.z -= moveSpeed;   // 下降
            break;
        case Qt::Key_E:
            pos.z += moveSpeed;   // 上升
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

    camera()->setPosition(pos);
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
    if (!m_isMousePressed || !camera()) return;

    float currentX = static_cast<float>(event->position().x());
    float currentY = static_cast<float>(event->position().y());
    float deltaX = currentX - m_lastMouseX;
    float deltaY = currentY - m_lastMouseY;

    glm::vec3 pos = camera()->getPosition();
    glm::vec3 target = camera()->getTarget();

    if (m_mouseButton == Qt::LeftButton) {
        // 左键：旋转相机视角（绕目标点轨道运动，球坐标以竖直轴 Z 为极轴）
        float pitch = deltaY * m_rotationSensitivity;
        float yaw = deltaX * m_rotationSensitivity;

        glm::vec3 offset = pos - target;
        float radius = glm::length(offset);
        if (radius < 1e-5f) return;

        // 方位角绕 Z（水平面内），极角自 +Z 轴量起
        float phi   = glm::atan(offset.y, offset.x) + yaw;
        float theta = glm::acos(glm::clamp(offset.z / radius, -1.0f, 1.0f)) - pitch;

        // 极角限幅：避免视线与竖直轴重合导致 lookAt 退化（标准轨道保护，非万向死锁）
        theta = glm::clamp(theta, 0.1f, glm::pi<float>() - 0.1f);

        offset.x = radius * glm::sin(theta) * glm::cos(phi);
        offset.y = radius * glm::sin(theta) * glm::sin(phi);
        offset.z = radius * glm::cos(theta);

        pos = target + offset;
        camera()->setPosition(pos);

    } else if (m_mouseButton == Qt::RightButton) {
        // 右键：平移相机（沿视平面移动，参考 up 为世界竖直轴 Z）
        glm::vec3 forward = glm::normalize(target - pos);
        glm::vec3 right = glm::cross(forward, glm::vec3(0.0f, 0.0f, 1.0f));
        if (glm::length(right) < 1e-4f) {
            right = glm::vec3(0.0f, 1.0f, 0.0f);   // 视线近竖直时兜底
        }
        right = glm::normalize(right);
        glm::vec3 up = glm::cross(right, forward);

        pos -= right * deltaX * m_panSensitivity * 10.0f;
        pos += up * deltaY * m_panSensitivity * 10.0f;
        target -= right * deltaX * m_panSensitivity * 10.0f;
        target += up * deltaY * m_panSensitivity * 10.0f;

        camera()->setPosition(pos);
        camera()->setTarget(target);
    }

    m_lastMouseX = currentX;
    m_lastMouseY = currentY;
    event->accept();
}

void QtViewer::wheelEvent(QWheelEvent* event)
{
    if (!camera()) {
        event->accept();
        return;
    }

    // 滚轮缩放（沿视线方向移动相机）
    float delta = static_cast<float>(event->angleDelta().y());
    glm::vec3 pos = camera()->getPosition();
    glm::vec3 target = camera()->getTarget();
    glm::vec3 dir = glm::normalize(target - pos);

    pos += dir * delta * m_zoomSensitivity;
    camera()->setPosition(pos);

    event->accept();
}

void QtViewer::resetTransform()
{
    if (camera()) {
        camera()->setPosition(glm::vec3(2.0f, 2.0f, 2.0f));
        camera()->setTarget(glm::vec3(0.0f, 0.0f, 0.0f));
    }
    m_autoRotate = true;
}
