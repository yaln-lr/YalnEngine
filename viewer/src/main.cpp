// 简化的 Qt Vulkan Viewer 主程序
// 使用 QtViewer 类（继承自 YalnView）

#include <QApplication>
#include <QVBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QStatusBar>
#include <QFrame>
#include <QMainWindow>
#include <QMessageBox>
#include <QElapsedTimer>
#include <iostream>

#include "qtviewer.h"
#include "yalnperspectivecamera.h"
#include "YalnMesh.h"

int main(int argc, char *argv[])
{
    // 创建 Qt 应用程序
    QApplication app(argc, argv);
    app.setApplicationName("Yaln Vulkan Viewer");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("YalnEngine");

    // 创建主窗口
    QMainWindow mainWindow;
    mainWindow.setWindowTitle("Yaln Vulkan Viewer - Qt Widget");
    mainWindow.resize(1280, 800);

    // 创建中央部件（包含 viewer 和状态栏）
    QFrame* centralFrame = new QFrame(&mainWindow);
    QVBoxLayout* mainLayout = new QVBoxLayout(centralFrame);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 创建 Vulkan Viewer
    QtViewer* viewer = new QtViewer(centralFrame);
    mainLayout->addWidget(viewer, 1);

    // 创建状态栏标签
    QLabel* statusLabel = new QLabel("Ready | FPS: --", centralFrame);
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->setStyleSheet("QLabel { padding: 4px; background: #2b2b2b; color: #ffffff; }");
    mainLayout->addWidget(statusLabel);

    // 创建菜单栏
    QMenuBar* menuBar = mainWindow.menuBar();

    // 文件菜单
    QMenu* fileMenu = menuBar->addMenu("&File");

    QAction* resetAction = new QAction("&Reset View", &mainWindow);
    resetAction->setShortcut(QKeySequence("R"));
    fileMenu->addAction(resetAction);

    fileMenu->addSeparator();

    QAction* exitAction = new QAction("E&xit", &mainWindow);
    exitAction->setShortcut(QKeySequence("Ctrl+Q"));
    fileMenu->addAction(exitAction);

    // 视图菜单
    QMenu* viewMenu = menuBar->addMenu("&View");

    QAction* toggleRotateAction = new QAction("&Auto Rotate", &mainWindow);
    toggleRotateAction->setCheckable(true);
    toggleRotateAction->setChecked(true);
    viewMenu->addAction(toggleRotateAction);

    // 帮助菜单
    QMenu* helpMenu = menuBar->addMenu("&Help");

    QAction* aboutAction = new QAction("&About", &mainWindow);
    helpMenu->addAction(aboutAction);

    // 连接信号和槽
    QObject::connect(resetAction, &QAction::triggered, [viewer]() {
        viewer->resetTransform();
    });

    QObject::connect(toggleRotateAction, &QAction::toggled, [viewer](bool checked) {
        viewer->setAutoRotate(checked);
    });

    QObject::connect(exitAction, &QAction::triggered, [=, &app]() {
        app.quit();
    });

    QObject::connect(aboutAction, &QAction::triggered, [&mainWindow]() {
        QMessageBox::about(&mainWindow, "About Yaln Vulkan Viewer",
            "<h3>Yaln Vulkan Viewer</h3>"
            "<p>A Qt Widget-based Vulkan renderer.</p>"
            "<p><b>Controls:</b><br>"
            "- Left Mouse: Orbit camera<br>"
            "- Right Mouse: Pan camera<br>"
            "- Middle Mouse / Scroll: Zoom<br>"
            "- WASD: Move camera position<br>"
            "- QE: Up/Down<br>"
            "- +/-: Dolly in/out<br>"
            "- Space: Toggle Auto-Rotate<br>"
            "- R: Reset View</p>"
        );
    });

    // FPS 计算
    QElapsedTimer fpsTimer;
    fpsTimer.start();
    int lastFrameCount = 0;
    QObject::connect(viewer, &QtViewer::frameRendered, [statusLabel, &fpsTimer, &lastFrameCount](int frameCount) {
        // 每秒更新一次 FPS
        if (fpsTimer.elapsed() >= 1000) {
            int fps = frameCount - lastFrameCount;
            statusLabel->setText(QString("Frame: %1 | FPS: %2").arg(frameCount).arg(fps));
            lastFrameCount = frameCount;
            fpsTimer.restart();
        }
    });

    QObject::connect(viewer, &QtViewer::vulkanInitialized, [statusLabel](bool success) {
        if (!success) {
            statusLabel->setText("Vulkan Initialization Failed!");
        } else {
            statusLabel->setText("Vulkan Initialized Successfully");
        }
    });

    QObject::connect(viewer, &QtViewer::errorOccurred, [statusLabel](const QString& error) {
        statusLabel->setText(QString("Error: %1").arg(error));
    });

    // 设置中央部件
    mainWindow.setCentralWidget(centralFrame);

    // 显示主窗口
    mainWindow.show();

    // 初始化 Vulkan
    if (!viewer->initialize("Yaln Vulkan Viewer")) {
        QMessageBox::critical(&mainWindow, "Error", "Failed to initialize Vulkan!");
        return 1;
    }

    // 设置相机（使用透视相机）
    float aspect = 1280.0f / 800.0f;
    YalnPerspectiveCameraPtr camera(new YalnPerspectiveCamera(45.0f, aspect, 0.1f, 100.0f));
    camera->setPosition(glm::vec3(2.0f, 2.0f, 2.0f));
    camera->setTarget(glm::vec3(0.0f, 0.0f, 0.0f));
    viewer->setCameta(camera);

    // 添加球体到场景
    YalnSphere sphere;
    sphere.m_position = glm::vec3(0.0f, 0.0f, 0.0f);
    viewer->addMesh(&sphere);

    // 添加正方体到球体左边
    YalnCube cube;
    cube.m_position = glm::vec3(-1.5f, 0.0f, 0.0f);
    cube.m_rotation = glm::vec3(0.0f, glm::radians(45.0f), 0.0f);  // 绕Y轴旋转45度
    viewer->addMesh(&cube);

    // 启动渲染循环
    viewer->startRenderLoop();

    // 进入 Qt 事件循环
    return app.exec();
}
