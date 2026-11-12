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
#include "yalnscene.h"
#include "yalnnode.h"
#include "yalndrawable.h"

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

    // ---- 开启深度测试（必须在 initialize 之前注入配置）----
    // 渲染通道：为每张交换链图像分配深度附件
    YalnRenderPassConfig rpc = YalnRenderPassConfig::makeDefault();
    rpc.enableDepth = true;
    viewer->setRenderPassConfig(rpc);

    // 管线：深度测试 + 深度写入（ compareOp 默认 eLessOrEqual ）
    // 注意约束：depthTest/depthWrite 要求渲染通道 enableDepth，组合校验会拦截不一致
    YalnPipelineConfig ppc = YalnPipelineConfig::makeDefault();
    ppc.depthTest  = true;
    ppc.depthWrite = true;
    viewer->setPipelineConfig(ppc);

    // 初始化 Vulkan
    if (!viewer->initialize("Yaln Vulkan Viewer")) {
        QMessageBox::critical(&mainWindow, "Error", "Failed to initialize Vulkan!\n\n"
                              + QString::fromStdString(viewer->lastError()));
        return 1;
    }

    // 设置相机（俯瞰整个村庄）
    float aspect = 1280.0f / 800.0f;
    YalnPerspectiveCameraPtr camera(new YalnPerspectiveCamera(45.0f, aspect, 0.1f, 100.0f));
    camera->setPosition(glm::vec3(6.5f, -7.0f, 5.0f));
    camera->setTarget(glm::vec3(0.0f, 0.0f, 0.6f));
    viewer->setCameta(camera);

    // ==================== 场景树：3 栋房子 + 完整地面 ====================
    //
    // Root（组节点）
    //  ├─ ground（叶子节点）        ── 完整地面（整块薄板）
    //  ├─ house1（组节点，位置/旋转/缩放）
    //  │    ├─ walls（叶子节点）    ── 墙体绘制项
    //  │    └─ roof（叶子节点）     ── 屋顶绘制项
    //  ├─ house2 ...
    //  └─ house3 ...
    //
    // 三栋房子共享同一份墙体/屋顶网格 → GPU 侧只有 3 份缓冲（地面/墙/屋顶），
    // 每栋房子靠节点变换实现不同位置、朝向与大小。

    // 网格资源（栈上分配，生命周期覆盖整个渲染循环）
    YalnCube    groundMesh(14.0f, 14.0f, 0.2f);    // 完整地面：整块薄板
    YalnCube    wallMesh(2.0f, 2.0f, 1.2f);        // 共享墙体
    YalnPyramid roofMesh(2.8f, 2.8f, 0.9f);        // 共享屋顶（四棱锥）
    YalnCube    poleMesh(0.25f, 0.25f, 2.2f);      // 风车杆
    YalnCube    bladeMesh(2.2f, 0.2f, 0.3f);       // 风车叶（十字双叶用同一网格两次自旋 90°? 简化：单叶双份对置）
    YalnCube    bladeMesh2(0.2f, 2.2f, 0.3f);      // 风车叶（另一方向）
    YalnCube    markerMesh(0.06f, 1.2f, 0.8f);     // 公告板薄片（法线朝 +X）
    YalnCube    flatRoofMesh(2.4f, 2.4f, 0.25f);   // 平顶屋顶备选
    YalnSphere  rockMesh(0.45f, 24);               // 岩石

    IYalnScene& scene = viewer->scene();
    YalnNode* root = scene.root();

    // ---- 地面（叶子节点只能包含可绘制对象）----
    auto groundNode = std::make_unique<YalnLeafNode>("ground");
    auto groundDrawable = std::make_unique<YalnMeshDrawable>(&groundMesh);
    groundDrawable->setPosition(glm::vec3(0.0f, 0.0f, -0.1f));   // 顶面抬到 z=0
    groundNode->attachDrawable(std::move(groundDrawable));
    root->addChild(std::move(groundNode));

    // ---- 三栋房子（组节点组织，墙体/屋顶是叶子节点）----
    struct HouseSpec {
        glm::vec3 position;
        float     rotationZ;   // 弧度
        float     scale;
    };
    const HouseSpec houses[] = {
        { glm::vec3(-3.0f, -2.0f, 0.0f), glm::radians( 15.0f), 1.0f },
        { glm::vec3( 2.5f,  2.0f, 0.0f), glm::radians(-25.0f), 0.8f },
        { glm::vec3( 3.0f, -2.5f, 0.0f), glm::radians( 50.0f), 1.2f },
    };

    int houseIndex = 1;
    for (const HouseSpec& spec : houses)
    {
        const std::string base = "house" + std::to_string(houseIndex++);

        auto house = std::make_unique<YalnGroupNode>(base);
        house->setPosition(spec.position);
        house->setRotation(glm::vec3(0.0f, 0.0f, spec.rotationZ));
        house->setScale(glm::vec3(spec.scale, spec.scale, spec.scale));

        // 墙体（本地偏移：墙体网格中心在原点，抬高半高使底面贴地）
        auto walls = std::make_unique<YalnLeafNode>(base + ".walls");
        auto wallDrawable = std::make_unique<YalnMeshDrawable>(&wallMesh);
        wallDrawable->setPosition(glm::vec3(0.0f, 0.0f, 0.6f));
        walls->attachDrawable(std::move(wallDrawable));

        // 屋顶：第 2 栋（house2）用开关节点提供两种屋顶方案（默认尖顶，可切平顶）
        std::unique_ptr<YalnNode> roof;
        if (houseIndex == 3)   // houseIndex 在本轮开头已自增：house2 对应值 3
        {
            auto roofSwitch = std::make_unique<YalnSwitchNode>(base + ".roofStyle");

            auto roofPyramid = std::make_unique<YalnLeafNode>(base + ".roofPyramid");
            auto pyramidDrawable = std::make_unique<YalnMeshDrawable>(&roofMesh);
            pyramidDrawable->setPosition(glm::vec3(0.0f, 0.0f, 1.65f));
            roofPyramid->attachDrawable(std::move(pyramidDrawable));

            auto roofFlat = std::make_unique<YalnLeafNode>(base + ".roofFlat");
            auto flatDrawable = std::make_unique<YalnMeshDrawable>(&flatRoofMesh);
            flatDrawable->setPosition(glm::vec3(0.0f, 0.0f, 1.325f));
            roofFlat->attachDrawable(std::move(flatDrawable));

            roofSwitch->addChild(std::move(roofPyramid));
            roofSwitch->addChild(std::move(roofFlat));
            roofSwitch->setActiveChild(base + ".roofPyramid");
            roof = std::move(roofSwitch);
        }
        else
        {
            auto roofLeaf = std::make_unique<YalnLeafNode>(base + ".roof");
            auto roofDrawable = std::make_unique<YalnMeshDrawable>(&roofMesh);
            roofDrawable->setPosition(glm::vec3(0.0f, 0.0f, 1.65f));
            roofLeaf->attachDrawable(std::move(roofDrawable));
            roof = std::move(roofLeaf);
        }

        house->addChild(std::move(walls));
        house->addChild(std::move(roof));
        root->addChild(std::move(house));
    }

    // ---- 风车：TranslateNode 定位 + SpinNode 自旋叶轮 ----
    auto windmill = std::make_unique<YalnGroupNode>("windmill");

    auto windmillPos = std::make_unique<YalnTranslateNode>("windmill.pos");
    windmillPos->setOffset(glm::vec3(-0.5f, 3.5f, 0.0f));

    auto windmillBody = std::make_unique<YalnLeafNode>("windmill.base");
    auto poleDrawable = std::make_unique<YalnMeshDrawable>(&poleMesh);
    poleDrawable->setPosition(glm::vec3(0.0f, 0.0f, 1.1f));
    windmillBody->attachDrawable(std::move(poleDrawable));

    // 叶轮挂在杆顶（z=2.2），绕水平 Y 轴持续自旋，双叶十字布局
    auto rotor = std::make_unique<YalnSpinNode>("windmill.rotor");
    rotor->setPosition(glm::vec3(0.0f, 0.0f, 2.2f));
    rotor->setAxis(glm::vec3(0.0f, 1.0f, 0.0f));
    rotor->setSpeed(glm::radians(120.0f));

    auto bladeA = std::make_unique<YalnLeafNode>("windmill.bladeA");
    auto bladeADrawable = std::make_unique<YalnMeshDrawable>(&bladeMesh);
    bladeADrawable->setPosition(glm::vec3(0.0f, 0.0f, 0.0f));
    bladeA->attachDrawable(std::move(bladeADrawable));

    auto bladeB = std::make_unique<YalnLeafNode>("windmill.bladeB");
    auto bladeBDrawable = std::make_unique<YalnMeshDrawable>(&bladeMesh2);
    bladeBDrawable->setPosition(glm::vec3(0.0f, 0.0f, 0.0f));
    bladeB->attachDrawable(std::move(bladeBDrawable));

    rotor->addChild(std::move(bladeA));
    rotor->addChild(std::move(bladeB));

    windmillPos->addChild(std::move(windmillBody));
    windmillPos->addChild(std::move(rotor));
    windmill->addChild(std::move(windmillPos));
    root->addChild(std::move(windmill));

    // ---- 公告板标记：始终水平朝向相机的薄片 ----
    auto marker = std::make_unique<YalnBillboardNode>("marker");
    marker->setPosition(glm::vec3(-0.5f, 3.5f, 3.4f));   // 风车顶部上方
    auto markerDrawable = std::make_unique<YalnMeshDrawable>(&markerMesh);
    marker->attachDrawable(std::move(markerDrawable));
    root->addChild(std::move(marker));

    // ---- 岩石：变换节点链 位移 → 倾斜 → 缩放 → 网格 ----
    auto rock = std::make_unique<YalnTranslateNode>("rock");
    rock->setOffset(glm::vec3(4.5f, 0.5f, 0.0f));

    auto rockTilt = std::make_unique<YalnRotateNode>("rock.tilt");
    rockTilt->setAxis(glm::vec3(0.0f, 1.0f, 0.0f));
    rockTilt->setAngle(glm::radians(-18.0f));

    auto rockSize = std::make_unique<YalnScaleNode>("rock.size");
    rockSize->setScaleFactor(glm::vec3(1.3f, 1.0f, 0.8f));

    auto rockBody = std::make_unique<YalnLeafNode>("rock.body");
    auto rockDrawable = std::make_unique<YalnMeshDrawable>(&rockMesh);
    rockDrawable->setPosition(glm::vec3(0.0f, 0.0f, 0.35f));
    rockBody->attachDrawable(std::move(rockDrawable));

    rockSize->addChild(std::move(rockBody));
    rockTilt->addChild(std::move(rockSize));
    rock->addChild(std::move(rockTilt));
    root->addChild(std::move(rock));

    std::cout << "Scene tree: nodes=" << root->countNodes()
              << ", drawables=" << root->countDrawables() << std::endl;

    // 场景树就绪，重建 GPU 缓冲
    if (!viewer->rebuildScene()) {
        QMessageBox::warning(&mainWindow, "Warning", "Failed to build scene GPU buffers!");
    }

    // 启动渲染循环
    viewer->startRenderLoop();

    // 进入 Qt 事件循环
    return app.exec();
}
