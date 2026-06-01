#ifndef QTVIEWER_H
#define QTVIEWER_H

#include <QWidget>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QTimer>
#include <QPaintEvent>

#include "yalnview.h"

class QtViewer : public QWidget, public YalnView
{
    Q_OBJECT

public:
    QtViewer(QWidget* parent = nullptr);
    virtual ~QtViewer();

    // 启动渲染循环
    void startRenderLoop();

    // 初始化 Vulkan
    bool initialize(const char* appName = "QtViewer App");

signals:
    void frameRendered(int frameIndex);
    void vulkanInitialized(bool success);
    void errorOccurred(const QString& error);

public slots:
    void onFrameTimeout();

protected:
    // YalnView 抽象方法实现
    virtual bool createSurface() override;
    virtual std::vector<const char*> getRequiredInstanceExtensions() override;

    // Qt 事件处理
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void handleKeyInput(int key, bool pressed);

public:
    // 公共访问方法
    void resetTransform();
    void setAutoRotate(bool enabled) { m_autoRotate = enabled; }
    bool isAutoRotate() const { return m_autoRotate; }
    void setAutoRotateSpeed(float speed) { m_autoRotateSpeed = speed; }
    float getAutoRotateSpeed() const { return m_autoRotateSpeed; }
    YalnCameraPtr getCamera() const { return m_camera_ptr; }
    void addMesh(YalnMesh* mesh) { YalnView::addMesh(mesh); }

private:
    QTimer* m_renderTimer = nullptr;

    bool m_autoRotate = true;
    float m_autoRotateSpeed = 1.0f;

    bool m_isMousePressed = false;
    float m_lastMouseX = 0.0f;
    float m_lastMouseY = 0.0f;
    int m_mouseButton = -1;

    float m_rotationSensitivity = 0.005f;
    float m_panSensitivity = 0.005f;
    float m_zoomSensitivity = 0.002f;

    uint32_t m_frameCount = 0;
    uint32_t m_width = 800;
    uint32_t m_height = 600;
};

#endif // QTVIEWER_H
