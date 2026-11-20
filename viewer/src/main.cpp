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
#include <string>

#include "qtviewer.h"
#include "yalnperspectivecamera.h"
#include "YalnMesh.h"
#include "yalnscene.h"
#include "yalnnode.h"
#include "yalndrawable.h"
#include "yalnmath.h"
#include "yalnlight.h"

// 构建村庄场景（网格用 static 局部对象：首次加载时构造，程序全程存活；
// 重复加载时先 scene.clear() 清空旧树与灯光，再复用同一批网格重建）
static void buildVillageScene(QtViewer* viewer);
static void applyRenderExtensionDemo(QtViewer* viewer);

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

    // 场景菜单：启动时空场景，点"加载村庄场景"后才构建并显示
    QMenu* sceneMenu = menuBar->addMenu(QString::fromUtf8("\u573a\u666f(&S)"));

    QAction* loadSceneAction = new QAction(QString::fromUtf8("\u52a0\u8f7d\u6751\u5e84\u573a\u666f"), &mainWindow);
    loadSceneAction->setShortcut(QKeySequence("Ctrl+L"));
    sceneMenu->addAction(loadSceneAction);

    QAction* demoAction = new QAction(QString::fromUtf8("\u5e94\u7528\u6750\u8d28/\u7ba1\u7ebf\u793a\u4f8b"), &mainWindow);
    sceneMenu->addAction(demoAction);

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

    QObject::connect(loadSceneAction, &QAction::triggered, [viewer, statusLabel, &mainWindow]() {
        buildVillageScene(viewer);
        if (viewer->rebuildScene()) {
            statusLabel->setText(QString::fromUtf8("\u573a\u666f\u5df2\u52a0\u8f7d"));
        } else {
            QMessageBox::warning(&mainWindow, "Warning", "Failed to build scene GPU buffers!");
        }
    });

    QObject::connect(demoAction, &QAction::triggered, [viewer, statusLabel]() {
        if (!viewer->isInitialized() || viewer->scene().isEmpty()) {
            statusLabel->setText("Load the village scene first");
            return;
        }
        applyRenderExtensionDemo(viewer);
        statusLabel->setText("Render extension demo applied");
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

    // 设置相机（俯瞰整个村庄的默认视角；场景加载后即为此视角）
    float aspect = 1280.0f / 800.0f;
    YalnPerspectiveCameraPtr camera(new YalnPerspectiveCamera(45.0f, aspect, 0.1f, 100.0f));
    camera->setPosition(glm::vec3(16.0f, -18.0f, 14.0f));
    camera->setTarget(glm::vec3(0.0f, 0.0f, 1.0f));
    viewer->setCameta(camera);

    // 可选：命令行直接加载场景并应用材质/管线示例，便于自动化验证。
    if (app.arguments().contains("--demo-render-extension")) {
        buildVillageScene(viewer);
        if (viewer->rebuildScene()) {
            applyRenderExtensionDemo(viewer);
        }
    }

    // 启动渲染循环（此时场景尚未加载：每帧仅清屏，等待用户从菜单加载）
    viewer->startRenderLoop();

    // 进入 Qt 事件循环
    return app.exec();
}

// ==================== 场景构建：村庄 ====================
//
// 房子由多面墙拼成（前/后/左右四面墙 + 门 + 四扇窗 + 屋顶），
// 门/窗略微凸出墙面避免与墙共面闪烁；树 = 树干 + 双层球冠；
// 路 = 两条略高于地面的灰色板。所有房子/树共享网格，GPU 缓冲去重。
// GPU 缓冲由调用方在函数返回后通过 rebuildScene() 重建。
static void buildVillageScene(QtViewer* viewer)
{
    IYalnScene& scene = viewer->scene();
    // 重复加载：先清空旧场景树与灯光（网格为 static，可复用）
    scene.clear();
    YalnNode* root = scene.root();

    // ==================== 场景树：村庄（多面墙房子 + 门 + 窗 + 路 + 树 + 风车）====================
    //
    // 房子由多面墙拼成（前/后/左右四面墙 + 门 + 四扇窗 + 屋顶），
    // 门/窗略微凸出墙面避免与墙共面闪烁；树 = 树干 + 双层球冠；
    // 路 = 两条略高于地面的灰色板。所有房子/树共享网格，GPU 缓冲去重。

    // ---- 网格资源（栈上分配，生命周期覆盖整个渲染循环）----
    // 配色：自然村庄色板（低饱和大地色系，同类物体色相一致，仅叶轮/屋顶用少量强调色）
    static YalnCube    groundMesh(40.0f, 40.0f, 0.2f, glm::vec3(0.42f, 0.55f, 0.33f));   // 草地（柔和草绿）
    // 水系与地形
    static YalnCube    lakeCenterMesh(13.0f, 13.0f, 0.03f, glm::vec3(0.30f, 0.48f, 0.62f));  // 湖心盘（湖蓝）
    static YalnCube    lakeSegMesh(14.0f, 7.0f, 0.03f, glm::vec3(0.30f, 0.48f, 0.62f));      // 湖缘弧段：多角度旋转叠出近似圆形湖岸
    static YalnCube    riverMesh(5.5f, 2.2f, 0.03f, glm::vec3(0.34f, 0.52f, 0.66f));     // 河段（略浅）
    static YalnCube    bridgeMesh(1.6f, 2.8f, 0.08f, glm::vec3(0.50f, 0.36f, 0.24f));    // 桥面（木色）
    static YalnCube    bridgePostMesh(0.14f, 0.14f, 0.5f, glm::vec3(0.42f, 0.30f, 0.20f));
    static YalnPyramid mountainMesh(5.0f, 5.0f, 3.2f, glm::vec3(0.47f, 0.46f, 0.43f));   // 山体（灰岩）
    static YalnPyramid snowCapMesh(2.0f, 2.0f, 1.3f, glm::vec3(0.94f, 0.95f, 0.97f));    // 雪帽
    // 电力设施
    static YalnCube    ppoleMesh(0.12f, 0.12f, 2.0f, glm::vec3(0.38f, 0.30f, 0.22f));    // 电线杆（深木棕）
    static YalnCube    crossarmMesh(0.08f, 0.9f, 0.08f, glm::vec3(0.38f, 0.30f, 0.22f)); // 横担
    static YalnCube    wireMesh(7.0f, 0.025f, 0.025f, glm::vec3(0.22f, 0.22f, 0.24f));   // 电线（深灰，跨 7m 杆距）
    // 植被
    static YalnSphere  bushMesh(0.22f, 12, glm::vec3(0.30f, 0.44f, 0.24f));              // 灌木（暗绿）
    static YalnSphere  bushMesh2(0.16f, 10, glm::vec3(0.34f, 0.48f, 0.27f));             // 小灌木
    static YalnSphere  rockSmallMesh(0.2f, 10, glm::vec3(0.50f, 0.49f, 0.47f));          // 小石头
    // 房子部件（三栋共享）
    static YalnCube    wallFrontMesh(2.4f, 0.15f, 1.6f, glm::vec3(0.91f, 0.87f, 0.78f)); // 前/后墙（米白）
    static YalnCube    wallSideMesh(0.15f, 1.7f, 1.6f, glm::vec3(0.86f, 0.81f, 0.70f));  // 左/右墙（阴影侧略深）
    static YalnPyramid roofMesh(2.8f, 2.5f, 0.8f, glm::vec3(0.68f, 0.38f, 0.30f));       // 尖顶（陶土红）
    static YalnCube    flatRoofMesh(2.6f, 2.3f, 0.25f, glm::vec3(0.45f, 0.40f, 0.36f));  // 平顶（深灰棕）
    static YalnCube    doorMesh(0.7f, 0.10f, 1.3f, glm::vec3(0.40f, 0.28f, 0.18f));      // 门（深棕）
    static YalnCube    windowMeshF(0.6f, 0.10f, 0.5f, glm::vec3(0.58f, 0.72f, 0.82f));   // 前/后墙窗（柔天蓝）
    static YalnCube    windowMeshS(0.10f, 0.6f, 0.5f, glm::vec3(0.58f, 0.72f, 0.82f));   // 侧墙窗
    // 风车
    static YalnCube    poleMesh(0.25f, 0.25f, 2.2f, glm::vec3(0.48f, 0.48f, 0.50f));     // 杆（深灰）
    static YalnCube    bladeMesh(2.2f, 0.2f, 0.3f, glm::vec3(0.93f, 0.92f, 0.90f));      // 横叶（暖白，沿 X）
    static YalnCube    bladeMesh2(0.3f, 0.2f, 2.2f, glm::vec3(0.72f, 0.38f, 0.32f));     // 竖叶（砖红强调，沿 Z）
    static YalnCube    markerMesh(0.06f, 1.2f, 0.8f, glm::vec3(0.93f, 0.92f, 0.90f));    // 公告板（暖白）
    // 路
    static YalnCube    roadMeshNS(1.6f, 32.0f, 0.04f, glm::vec3(0.45f, 0.45f, 0.47f));   // 南北向（沥青灰）
    static YalnCube    roadMeshEW(38.0f, 1.6f, 0.04f, glm::vec3(0.48f, 0.48f, 0.50f));   // 东西向（略浅）
    // 树（共享树干/树冠）
    static YalnCube    trunkMesh(0.18f, 0.18f, 0.8f, glm::vec3(0.42f, 0.33f, 0.24f));    // 树干（灰棕）
    static YalnSphere  canopyMeshA(0.52f, 20, glm::vec3(0.34f, 0.47f, 0.28f));           // 下层树冠（暗绿）
    static YalnSphere  canopyMeshB(0.38f, 16, glm::vec3(0.40f, 0.53f, 0.32f));           // 上层树冠（受光绿）
    // 岩石
    static YalnSphere  rockMesh(0.45f, 24, glm::vec3(0.52f, 0.51f, 0.49f));              // 岩石（石灰岩灰）
    // 灯具
    static YalnSphere  lampBulbMesh(0.12f, 12, glm::vec3(1.0f, 0.92f, 0.62f));           // 路灯灯泡（高亮暖白）

    // ---- 可复用子树工厂：树 = 树干 + 双层球冠 ----
    auto makeTree = [&trunkMesh, &canopyMeshA, &canopyMeshB](const std::string& name, float scale) {
        auto tree = std::make_unique<YalnGroupNode>(name);
        tree->setScale(Eigen::Vector3f::Constant(scale));

        auto trunk = std::make_unique<YalnLeafNode>(name + ".trunk");
        auto trunkDrawable = std::make_unique<YalnMeshDrawable>(&trunkMesh);
        trunkDrawable->setPosition(Eigen::Vector3f(0.0f, 0.0f, 0.4f));
        trunk->attachDrawable(std::move(trunkDrawable));

        auto canopyA = std::make_unique<YalnLeafNode>(name + ".canopyA");
        auto canopyADrawable = std::make_unique<YalnMeshDrawable>(&canopyMeshA);
        canopyADrawable->setPosition(Eigen::Vector3f(0.0f, 0.0f, 1.15f));
        canopyA->attachDrawable(std::move(canopyADrawable));

        auto canopyB = std::make_unique<YalnLeafNode>(name + ".canopyB");
        auto canopyBDrawable = std::make_unique<YalnMeshDrawable>(&canopyMeshB);
        canopyBDrawable->setPosition(Eigen::Vector3f(0.12f, -0.08f, 1.5f));
        canopyB->attachDrawable(std::move(canopyBDrawable));

        tree->addChild(std::move(trunk));
        tree->addChild(std::move(canopyA));
        tree->addChild(std::move(canopyB));
        return tree;
    };

    // ---- 可复用子树工厂：房子 = 四面墙 + 门 + 四扇窗 + 屋顶 ----
    // 占地 2.4×2.0，墙厚 0.15，墙高 1.6；门窗嵌入墙面并凸出约 0.03 防共面闪烁
    auto makeHouse = [&wallFrontMesh, &wallSideMesh, &doorMesh, &windowMeshF, &windowMeshS,
                      &roofMesh, &flatRoofMesh](const std::string& name, bool withFlatRoofOption) {
        auto house = std::make_unique<YalnGroupNode>(name);
        constexpr float kWallH = 1.6f;

        auto addLeaf = [&house](const std::string& leafName, YalnMesh* mesh,
                                const Eigen::Vector3f& pos) {
            auto leaf = std::make_unique<YalnLeafNode>(leafName);
            auto d = std::make_unique<YalnMeshDrawable>(mesh);
            d->setPosition(pos);
            leaf->attachDrawable(std::move(d));
            house->addChild(std::move(leaf));
        };

        // 四面墙
        addLeaf(name + ".wallF", &wallFrontMesh, Eigen::Vector3f( 0.0f,  -0.925f, kWallH * 0.5f));
        addLeaf(name + ".wallB", &wallFrontMesh, Eigen::Vector3f( 0.0f,   0.925f, kWallH * 0.5f));
        addLeaf(name + ".wallL", &wallSideMesh,  Eigen::Vector3f(-1.125f,  0.0f,  kWallH * 0.5f));
        addLeaf(name + ".wallR", &wallSideMesh,  Eigen::Vector3f( 1.125f,  0.0f,  kWallH * 0.5f));

        // 门（前墙右半侧，底边贴地）
        addLeaf(name + ".door",  &doorMesh,    Eigen::Vector3f( 0.55f, -0.98f, 0.65f));
        // 前/后墙窗（左半侧）
        addLeaf(name + ".winF",  &windowMeshF, Eigen::Vector3f(-0.55f, -0.98f, 1.0f));
        addLeaf(name + ".winB",  &windowMeshF, Eigen::Vector3f(-0.55f,  0.98f, 1.0f));
        // 左/右侧墙窗
        addLeaf(name + ".winL",  &windowMeshS, Eigen::Vector3f(-1.16f,  0.0f,  1.0f));
        addLeaf(name + ".winR",  &windowMeshS, Eigen::Vector3f( 1.16f,  0.0f,  1.0f));

        // 屋顶：底面精确落在墙顶 z=1.6（金字塔半高 0.4 → 挂点 z=2.0）
        std::unique_ptr<YalnNode> roof;
        if (withFlatRoofOption)
        {
            auto roofSwitch = std::make_unique<YalnSwitchNode>(name + ".roofStyle");

            auto roofPyramid = std::make_unique<YalnLeafNode>(name + ".roofPyramid");
            auto pyramidDrawable = std::make_unique<YalnMeshDrawable>(&roofMesh);
            pyramidDrawable->setPosition(Eigen::Vector3f(0.0f, 0.0f, 2.0f));
            roofPyramid->attachDrawable(std::move(pyramidDrawable));

            auto roofFlat = std::make_unique<YalnLeafNode>(name + ".roofFlat");
            auto flatDrawable = std::make_unique<YalnMeshDrawable>(&flatRoofMesh);
            flatDrawable->setPosition(Eigen::Vector3f(0.0f, 0.0f, 1.725f));   // 平顶盖 [1.6, 1.85]
            roofFlat->attachDrawable(std::move(flatDrawable));

            roofSwitch->addChild(std::move(roofPyramid));
            roofSwitch->addChild(std::move(roofFlat));
            roofSwitch->setActiveChild(name + ".roofPyramid");
            roof = std::move(roofSwitch);
        }
        else
        {
            auto roofLeaf = std::make_unique<YalnLeafNode>(name + ".roof");
            auto roofDrawable = std::make_unique<YalnMeshDrawable>(&roofMesh);
            roofDrawable->setPosition(Eigen::Vector3f(0.0f, 0.0f, 2.0f));
            roofLeaf->attachDrawable(std::move(roofDrawable));
            roof = std::move(roofLeaf);
        }
        house->addChild(std::move(roof));
        return house;
    };

    // ---- 地面（叶子节点只能包含可绘制对象）----
    auto groundNode = std::make_unique<YalnLeafNode>("ground");
    auto groundDrawable = std::make_unique<YalnMeshDrawable>(&groundMesh);
    groundDrawable->setPosition(Eigen::Vector3f(0.0f, 0.0f, -0.1f));   // 顶面抬到 z=0
    groundNode->attachDrawable(std::move(groundDrawable));
    root->addChild(std::move(groundNode));

    // ---- 路：十字路（略高于草地，避免共面）----
    {
        auto roadNS = std::make_unique<YalnLeafNode>("road.ns");
        auto roadNSDrawable = std::make_unique<YalnMeshDrawable>(&roadMeshNS);
        roadNSDrawable->setPosition(Eigen::Vector3f(-2.0f, -3.5f, 0.025f));
        roadNS->attachDrawable(std::move(roadNSDrawable));
        root->addChild(std::move(roadNS));

        auto roadEW = std::make_unique<YalnLeafNode>("road.ew");
        auto roadEWDrawable = std::make_unique<YalnMeshDrawable>(&roadMeshEW);
        roadEWDrawable->setPosition(Eigen::Vector3f(0.0f, 1.0f, 0.025f));
        roadEW->attachDrawable(std::move(roadEWDrawable));
        root->addChild(std::move(roadEW));
    }

    // ---- 水系：蜿蜒河流（自西向东入湖）+ 弧形湖岸的湖 + 过路木桥 ----
    // 湖岸：湖心盘 + 6 条以湖心为轴每 30° 旋转的矩形弧段，叠合出近似圆形边缘
    {
        const std::pair<Eigen::Vector3f, float> riverSpans[] = {
            { Eigen::Vector3f(-16.5f, -8.0f, 0.02f), glm::radians(  8.0f) },
            { Eigen::Vector3f(-11.5f, -8.6f, 0.02f), glm::radians(-10.0f) },
            { Eigen::Vector3f( -6.5f, -8.2f, 0.02f), glm::radians(  6.0f) },
            { Eigen::Vector3f( -2.0f, -8.8f, 0.02f), glm::radians( -6.0f) },
            { Eigen::Vector3f(  1.5f, -9.0f, 0.02f), glm::radians(  0.0f) },
        };
        for (size_t i = 0; i < std::size(riverSpans); i++)
        {
            auto seg = std::make_unique<YalnLeafNode>("river" + std::to_string(i + 1));
            auto segDrawable = std::make_unique<YalnMeshDrawable>(&riverMesh);
            segDrawable->setPosition(riverSpans[i].first);
            seg->attachDrawable(std::move(segDrawable));
            seg->setEulerRotation(Eigen::Vector3f(0.0f, 0.0f, riverSpans[i].second));
            root->addChild(std::move(seg));
        }

        // 湖：中心 (8, -9)，近似圆形，半径约 7.5
        const Eigen::Vector3f lakeCenter(8.0f, -9.0f, 0.02f);
        auto lakeBody = std::make_unique<YalnGroupNode>("lake");
        auto lakeCenterNode = std::make_unique<YalnLeafNode>("lake.center");
        auto lakeCenterDrawable = std::make_unique<YalnMeshDrawable>(&lakeCenterMesh);
        lakeCenterDrawable->setPosition(lakeCenter);
        lakeCenterNode->attachDrawable(std::move(lakeCenterDrawable));
        lakeBody->addChild(std::move(lakeCenterNode));

        for (int i = 0; i < 6; i++)
        {
            auto seg = std::make_unique<YalnLeafNode>("lake.seg" + std::to_string(i + 1));
            auto segDrawable = std::make_unique<YalnMeshDrawable>(&lakeSegMesh);
            segDrawable->setPosition(lakeCenter);
            seg->attachDrawable(std::move(segDrawable));
            seg->setEulerRotation(Eigen::Vector3f(0.0f, 0.0f, glm::radians(30.0f * i)));
            lakeBody->addChild(std::move(seg));
        }
        root->addChild(std::move(lakeBody));

        // 木桥：南北路跨河处，桥面高于路面，四根桥墩
        auto bridge = std::make_unique<YalnGroupNode>("bridge");
        bridge->setPosition(Eigen::Vector3f(-2.0f, -8.8f, 0.06f));

        auto deck = std::make_unique<YalnLeafNode>("bridge.deck");
        auto deckDrawable = std::make_unique<YalnMeshDrawable>(&bridgeMesh);
        deck->attachDrawable(std::move(deckDrawable));
        bridge->addChild(std::move(deck));

        const Eigen::Vector3f postOffsets[] = {
            Eigen::Vector3f(-0.70f, -1.30f, 0.19f), Eigen::Vector3f(0.70f, -1.30f, 0.19f),
            Eigen::Vector3f(-0.70f,  1.30f, 0.19f), Eigen::Vector3f(0.70f,  1.30f, 0.19f),
        };
        for (size_t i = 0; i < std::size(postOffsets); i++)
        {
            auto post = std::make_unique<YalnLeafNode>("bridge.post" + std::to_string(i + 1));
            auto postDrawable = std::make_unique<YalnMeshDrawable>(&bridgePostMesh);
            postDrawable->setPosition(postOffsets[i]);
            post->attachDrawable(std::move(postDrawable));
            bridge->addChild(std::move(post));
        }
        root->addChild(std::move(bridge));
    }

    // ---- 十五栋房子（40×40 大地图上疏散分布；全部核算过与湖/河/路的间距，无压水）----
    struct HouseSpec {
        Eigen::Vector3f position;
        float           rotationZ;   // 弧度
        float           scale;
    };
    const HouseSpec houses[] = {
        // 河北岸村庄（南北路西侧）
        { Eigen::Vector3f( -5.0f,  -4.0f, 0.0f), glm::radians( 15.0f), 1.00f },  // house1
        { Eigen::Vector3f( -8.0f,  -1.5f, 0.0f), glm::radians(-20.0f), 0.90f },  // house2（平顶开关）
        { Eigen::Vector3f( -5.5f,   4.0f, 0.0f), glm::radians( 10.0f), 1.10f },  // house3
        // 南北路东侧
        { Eigen::Vector3f(  1.0f,  -3.5f, 0.0f), glm::radians(-30.0f), 0.85f },  // house4
        { Eigen::Vector3f(  2.0f,   3.5f, 0.0f), glm::radians(  5.0f), 0.95f },  // house5
        // 东西路沿线
        { Eigen::Vector3f(  5.5f,   2.8f, 0.0f), glm::radians(-25.0f), 0.80f },  // house6
        { Eigen::Vector3f(  8.5f,   4.5f, 0.0f), glm::radians( 15.0f), 1.05f },  // house7
        { Eigen::Vector3f(  4.5f,   7.5f, 0.0f), glm::radians( 30.0f), 0.90f },  // house8
        { Eigen::Vector3f(-10.0f,   6.5f, 0.0f), glm::radians(-10.0f), 1.00f },  // house9（山脚）
        { Eigen::Vector3f( 12.0f,   7.0f, 0.0f), glm::radians( 20.0f), 0.95f },  // house10（山脚）
        // 湖北岸（湖心 (8,-9)，半径约 7.5，以下均留出岸线距离）
        { Eigen::Vector3f(  5.5f,  -1.0f, 0.0f), glm::radians( 40.0f), 1.10f },  // house11
        { Eigen::Vector3f( 13.5f,  -1.5f, 0.0f), glm::radians(-15.0f), 0.90f },  // house12
        { Eigen::Vector3f( 16.5f,   4.0f, 0.0f), glm::radians( 10.0f), 0.85f },  // house13
        // 西侧散居
        { Eigen::Vector3f(-14.0f,  -3.0f, 0.0f), glm::radians(-40.0f), 1.00f },  // house14
        { Eigen::Vector3f(-16.0f,   3.0f, 0.0f), glm::radians(-30.0f), 0.90f },  // house15
    };

    int houseIndex = 1;
    for (const HouseSpec& spec : houses)
    {
        auto house = makeHouse("house" + std::to_string(houseIndex), houseIndex == 2);
        house->setPosition(spec.position);
        house->setEulerRotation(Eigen::Vector3f(0.0f, 0.0f, spec.rotationZ));   // 内部转四元数
        house->setScale(Eigen::Vector3f::Constant(spec.scale));
        root->addChild(std::move(house));
        houseIndex++;
    }

    // ---- 山脉（北缘与东缘，金字塔体 + 雪顶扣峰）----
    auto makeMountain = [&mountainMesh, &snowCapMesh](const std::string& name,
                                                      const Eigen::Vector3f& pos, float scale) {
        auto m = std::make_unique<YalnGroupNode>(name);
        m->setPosition(pos);
        m->setScale(Eigen::Vector3f::Constant(scale));

        auto body = std::make_unique<YalnLeafNode>(name + ".body");
        auto bodyDrawable = std::make_unique<YalnMeshDrawable>(&mountainMesh);
        bodyDrawable->setPosition(Eigen::Vector3f(0.0f, 0.0f, 1.6f));    // 底面贴地
        body->attachDrawable(std::move(bodyDrawable));

        auto snow = std::make_unique<YalnLeafNode>(name + ".snow");
        auto snowDrawable = std::make_unique<YalnMeshDrawable>(&snowCapMesh);
        snowDrawable->setPosition(Eigen::Vector3f(0.0f, 0.0f, 2.55f));   // 雪帽扣住峰顶
        snow->attachDrawable(std::move(snowDrawable));

        m->addChild(std::move(body));
        m->addChild(std::move(snow));
        return m;
    };
    {
        const std::pair<Eigen::Vector3f, float> mountains[] = {
            { Eigen::Vector3f(-10.0f, 17.5f, 0.0f), 1.60f },
            { Eigen::Vector3f(  2.0f, 18.5f, 0.0f), 2.00f },
            { Eigen::Vector3f( 13.0f, 17.0f, 0.0f), 1.50f },
            { Eigen::Vector3f(-18.0f, 12.0f, 0.0f), 1.20f },
            { Eigen::Vector3f( 18.5f, 11.0f, 0.0f), 1.30f },
            { Eigen::Vector3f( 18.0f, 18.0f, 0.0f), 1.00f },
        };
        for (size_t i = 0; i < std::size(mountains); i++)
        {
            auto m = makeMountain("mountain" + std::to_string(i + 1),
                                  mountains[i].first, mountains[i].second);
            root->addChild(std::move(m));
        }
    }

    // ---- 八棵树（路旁、山脚、湖畔）----
    const std::pair<Eigen::Vector3f, float> trees[] = {
        { Eigen::Vector3f(-13.0f,  -6.5f, 0.0f), 1.1f },
        { Eigen::Vector3f(-18.0f,   0.0f, 0.0f), 1.0f },
        { Eigen::Vector3f(-18.0f,   8.0f, 0.0f), 1.0f },
        { Eigen::Vector3f( -9.0f,  10.0f, 0.0f), 0.9f },
        { Eigen::Vector3f( -1.0f,  11.0f, 0.0f), 1.0f },
        { Eigen::Vector3f(  7.0f,  11.0f, 0.0f), 1.1f },
        { Eigen::Vector3f( 15.0f,  11.5f, 0.0f), 0.9f },
        { Eigen::Vector3f( 18.0f,  -1.0f, 0.0f), 0.9f },
        { Eigen::Vector3f( 11.0f, -17.0f, 0.0f), 1.0f },
        { Eigen::Vector3f( -6.0f, -13.0f, 0.0f), 1.2f },
        { Eigen::Vector3f(-12.0f, -12.0f, 0.0f), 1.0f },
        { Eigen::Vector3f( 17.0f, -14.0f, 0.0f), 0.8f },
    };
    for (size_t i = 0; i < std::size(trees); i++)
    {
        auto tree = makeTree("tree" + std::to_string(i + 1), trees[i].second);
        tree->setPosition(trees[i].first);
        root->addChild(std::move(tree));
    }

    // ---- 灌木（房前屋后 / 湖畔 / 河岸）----
    const std::pair<Eigen::Vector3f, float> bushes[] = {
        { Eigen::Vector3f( -3.5f,  2.2f, 0.0f), 1.0f },
        { Eigen::Vector3f(  3.2f,  2.6f, 0.0f), 0.8f },
        { Eigen::Vector3f( -6.5f, -2.0f, 0.0f), 1.0f },
        { Eigen::Vector3f(  9.5f,  2.8f, 0.0f), 1.0f },
        { Eigen::Vector3f( 15.5f, -4.5f, 0.0f), 1.0f },
        { Eigen::Vector3f(  0.5f, -6.0f, 0.0f), 0.7f },
        { Eigen::Vector3f(-16.0f, -5.5f, 0.0f), 0.8f },
        { Eigen::Vector3f( -3.0f,-11.0f, 0.0f), 0.9f },
        { Eigen::Vector3f(  6.8f,  7.8f, 0.0f), 1.0f },
        { Eigen::Vector3f(-11.0f,  3.5f, 0.0f), 0.8f },
    };
    for (size_t i = 0; i < std::size(bushes); i++)
    {
        auto bush = std::make_unique<YalnLeafNode>("bush" + std::to_string(i + 1));
        auto bushDrawable = std::make_unique<YalnMeshDrawable>(i % 2 ? &bushMesh2 : &bushMesh);
        bushDrawable->setPosition(Eigen::Vector3f(
            bushes[i].first.x(), bushes[i].first.y(), 0.22f * bushes[i].second));
        bush->attachDrawable(std::move(bushDrawable));
        bush->setScale(Eigen::Vector3f::Constant(bushes[i].second));
        root->addChild(std::move(bush));
    }

    // ---- 岩石群（河岸 / 湖畔 / 野地）----
    {
        const Eigen::Vector3f rockSpots[] = {
            Eigen::Vector3f( -7.5f,  -6.5f, 0.0f), Eigen::Vector3f( -1.0f, -12.0f, 0.0f),
            Eigen::Vector3f( 18.0f, -16.0f, 0.0f), Eigen::Vector3f( 17.0f,  -8.5f, 0.0f),
            Eigen::Vector3f(-14.0f, -14.0f, 0.0f),
        };
        const float rockScales[] = { 1.0f, 0.8f, 0.9f, 1.1f, 0.9f };
        for (size_t i = 0; i < std::size(rockSpots); i++)
        {
            auto rock = std::make_unique<YalnLeafNode>("rock." + std::to_string(i + 1));
            auto rockDrawable = std::make_unique<YalnMeshDrawable>(&rockSmallMesh);
            rockDrawable->setPosition(Eigen::Vector3f(
                rockSpots[i].x(), rockSpots[i].y(), 0.2f * rockScales[i]));
            rock->attachDrawable(std::move(rockDrawable));
            rock->setScale(Eigen::Vector3f::Constant(rockScales[i]));
            root->addChild(std::move(rock));
        }
    }

    // ---- 电力设施：沿东西路北侧的电线杆（杆 + 横担）与两根电线 ----
    {
        const float wireY = 2.4f;
        const float poleXs[] = { -14.0f, -7.0f, 0.0f, 7.0f, 14.0f };
        for (size_t i = 0; i < std::size(poleXs); i++)
        {
            const std::string base = "pole" + std::to_string(i + 1);
            auto pole = std::make_unique<YalnGroupNode>(base);
            pole->setPosition(Eigen::Vector3f(poleXs[i], wireY, 0.0f));

            auto post = std::make_unique<YalnLeafNode>(base + ".post");
            auto postDrawable = std::make_unique<YalnMeshDrawable>(&ppoleMesh);
            postDrawable->setPosition(Eigen::Vector3f(0.0f, 0.0f, 1.0f));
            post->attachDrawable(std::move(postDrawable));

            auto arm = std::make_unique<YalnLeafNode>(base + ".arm");
            auto armDrawable = std::make_unique<YalnMeshDrawable>(&crossarmMesh);
            armDrawable->setPosition(Eigen::Vector3f(0.0f, 0.0f, 1.8f));
            arm->attachDrawable(std::move(armDrawable));

            pole->addChild(std::move(post));
            pole->addChild(std::move(arm));
            root->addChild(std::move(pole));
        }
        // 电线：相邻电线杆之间两根（横担两端）
        for (size_t i = 0; i + 1 < std::size(poleXs); i++)
        {
            const float cx = (poleXs[i] + poleXs[i + 1]) * 0.5f;
            for (int s = 0; s < 2; s++)
            {
                auto wire = std::make_unique<YalnLeafNode>(
                    "wire" + std::to_string(i + 1) + "_" + std::to_string(s + 1));
                auto wireDrawable = std::make_unique<YalnMeshDrawable>(&wireMesh);
                wireDrawable->setPosition(Eigen::Vector3f(
                    cx, wireY + (s == 0 ? -0.32f : 0.32f), 1.78f));
                wire->attachDrawable(std::move(wireDrawable));
                root->addChild(std::move(wire));
            }
        }
    }

    // ---- 风车：TranslateNode 定位 + SpinNode 自旋叶轮 ----
    auto windmill = std::make_unique<YalnGroupNode>("windmill");

    auto windmillPos = std::make_unique<YalnTranslateNode>("windmill.pos");
    windmillPos->setOffset(Eigen::Vector3f(-5.5f, 9.5f, 0.0f));

    auto windmillBody = std::make_unique<YalnLeafNode>("windmill.base");
    auto poleDrawable = std::make_unique<YalnMeshDrawable>(&poleMesh);
    poleDrawable->setPosition(Eigen::Vector3f(0.0f, 0.0f, 1.1f));
    windmillBody->attachDrawable(std::move(poleDrawable));

    // 叶轮挂在杆顶（z=2.2），绕水平 Y 轴持续自旋，双叶十字布局
    auto rotor = std::make_unique<YalnSpinNode>("windmill.rotor");
    rotor->setPosition(Eigen::Vector3f(0.0f, 0.0f, 2.2f));
    rotor->setAxis(Eigen::Vector3f(0.0f, 1.0f, 0.0f));
    rotor->setSpeed(glm::radians(120.0f));

    auto bladeA = std::make_unique<YalnLeafNode>("windmill.bladeA");
    auto bladeADrawable = std::make_unique<YalnMeshDrawable>(&bladeMesh);
    bladeADrawable->setPosition(Eigen::Vector3f(0.0f, 0.0f, 0.0f));
    bladeA->attachDrawable(std::move(bladeADrawable));

    auto bladeB = std::make_unique<YalnLeafNode>("windmill.bladeB");
    auto bladeBDrawable = std::make_unique<YalnMeshDrawable>(&bladeMesh2);
    bladeBDrawable->setPosition(Eigen::Vector3f(0.0f, 0.0f, 0.0f));
    bladeB->attachDrawable(std::move(bladeBDrawable));

    rotor->addChild(std::move(bladeA));
    rotor->addChild(std::move(bladeB));

    windmillPos->addChild(std::move(windmillBody));
    windmillPos->addChild(std::move(rotor));
    windmill->addChild(std::move(windmillPos));
    root->addChild(std::move(windmill));

    // ---- 公告板标记：始终水平朝向相机的薄片 ----
    auto marker = std::make_unique<YalnBillboardNode>("marker");
    marker->setPosition(Eigen::Vector3f(-5.5f, 9.5f, 3.4f));   // 风车顶部上方
    auto markerDrawable = std::make_unique<YalnMeshDrawable>(&markerMesh);
    marker->attachDrawable(std::move(markerDrawable));
    root->addChild(std::move(marker));

    // ---- 岩石：变换节点链 位移 → 倾斜 → 缩放 → 网格 ----
    auto rock = std::make_unique<YalnTranslateNode>("rock");
    rock->setOffset(Eigen::Vector3f(14.5f, 10.0f, 0.0f));

    auto rockTilt = std::make_unique<YalnRotateNode>("rock.tilt");
    rockTilt->setAxisAngle(Eigen::Vector3f(0.0f, 1.0f, 0.0f), glm::radians(-18.0f));

    auto rockSize = std::make_unique<YalnScaleNode>("rock.size");
    rockSize->setScaleFactor(Eigen::Vector3f(1.3f, 1.0f, 0.8f));

    auto rockBody = std::make_unique<YalnLeafNode>("rock.body");
    auto rockDrawable = std::make_unique<YalnMeshDrawable>(&rockMesh);
    rockDrawable->setPosition(Eigen::Vector3f(0.0f, 0.0f, 0.35f));
    rockBody->attachDrawable(std::move(rockDrawable));

    rockSize->addChild(std::move(rockBody));
    rockTilt->addChild(std::move(rockSize));
    rock->addChild(std::move(rockTilt));
    root->addChild(std::move(rock));

    // ---- 灯光：环境光 + 太阳平行光 + 两盏路灯（点光）+ 风车探照灯（聚光）----
    {
        // 环境光：偏冷天光（模拟天空散射），决定暗面不至于全黑
        auto ambient = std::make_unique<YalnAmbientLight>();
        ambient->setColor(glm::vec3(0.58f, 0.65f, 0.78f));
        ambient->setIntensity(0.55f);
        scene.addLight(std::move(ambient));

        // 太阳：暖白平行光，从东南上空斜射
        auto sun = std::make_unique<YalnDirectionalLight>();
        sun->setDirection(glm::normalize(glm::vec3(-0.45f, -0.35f, -0.9f)));
        sun->setColor(glm::vec3(1.00f, 0.96f, 0.88f));
        sun->setIntensity(0.9f);
        scene.addLight(std::move(sun));

        // 路灯：挂在东西路电线杆横担下（灯泡小球可视化 + 暖黄点光）
        const glm::vec3 lampSpots[] = {
            glm::vec3(0.0f, 2.4f, 1.62f),
            glm::vec3(7.0f, 2.4f, 1.62f),
        };
        for (int i = 0; i < 2; i++)
        {
            auto lampNode = std::make_unique<YalnLeafNode>("lamp." + std::to_string(i + 1));
            auto lampDrawable = std::make_unique<YalnMeshDrawable>(&lampBulbMesh);
            lampDrawable->setPosition(Eigen::Vector3f(lampSpots[i].x, lampSpots[i].y, lampSpots[i].z));
            lampNode->attachDrawable(std::move(lampDrawable));
            root->addChild(std::move(lampNode));

            auto lampLight = std::make_unique<YalnPointLight>();
            lampLight->setPosition(lampSpots[i]);
            lampLight->setColor(glm::vec3(1.0f, 0.85f, 0.55f));
            lampLight->setIntensity(1.3f);
            lampLight->setRange(8.0f);
            scene.addLight(std::move(lampLight));
        }

        // 风车探照灯：从杆顶斜向下照射的聚光灯（内锥 18°/外锥 30°，扫亮风车脚下草地）
        auto searchLight = std::make_unique<YalnSpotLight>();
        searchLight->setPosition(glm::vec3(-5.5f, 9.5f, 2.6f));
        searchLight->setDirection(glm::normalize(glm::vec3(0.15f, 0.25f, -1.0f)));
        searchLight->setColor(glm::vec3(1.0f, 0.95f, 0.85f));
        searchLight->setIntensity(1.6f);
        searchLight->setRange(14.0f);
        searchLight->setAngles(18.0f, 30.0f);
        scene.addLight(std::move(searchLight));
    }

    std::cout << "Scene tree: nodes=" << root->countNodes()
              << ", drawables=" << root->countDrawables() << std::endl;
}

// ==================== 扩展示例：材质与按对象管线 ====================
// 1) 风车组：继续使用默认 PSO，只换材质乘色（同管线、不同材质）。
// 2) 三座山：创建一条线框 PSO，并通过材质挂接到山体（不同对象、不同管线）。
static void applyRenderExtensionDemo(QtViewer* viewer)
{
    if (viewer == nullptr || !viewer->isInitialized() || viewer->scene().isEmpty()) {
        return;
    }

    static YalnMaterialHandle tintMaterial = YALN_INVALID_MATERIAL;
    if (tintMaterial == YALN_INVALID_MATERIAL) {
        YalnMaterialDesc desc;
        desc.name = "WindmillTint";
        desc.baseColor = glm::vec4(1.0f, 0.78f, 0.42f, 1.0f);
        desc.defaultPipeline = YALN_DEFAULT_PIPELINE;
        tintMaterial = viewer->createMaterial(desc);
    }
    if (YalnNode* windmill = viewer->scene().root()->find("windmill")) {
        windmill->setMaterialRecursive(tintMaterial);
    }

    static YalnPipelineHandle wirePipeline = YALN_INVALID_PIPELINE;
    static YalnMaterialHandle wireMaterial = YALN_INVALID_MATERIAL;
    if (wirePipeline == YALN_INVALID_PIPELINE) {
        if (viewer->device().physicalDevice().getFeatures().fillModeNonSolid) {
            YalnPipelineConfig config = viewer->pipeline().config();
            config.polygonMode = vk::PolygonMode::eLine;
            config.cullMode = vk::CullModeFlagBits::eNone;
            config.lineWidth = 1.0f;
            wirePipeline = viewer->createPipeline(config, "MountainWireframe");
        } else {
            std::cout << "fillModeNonSolid unsupported: skip wireframe demo" << std::endl;
        }
    }
    if (wirePipeline != YALN_INVALID_PIPELINE && wireMaterial == YALN_INVALID_MATERIAL) {
        YalnMaterialDesc desc;
        desc.name = "MountainWire";
        desc.baseColor = glm::vec4(0.40f, 1.0f, 0.70f, 1.0f);
        desc.defaultPipeline = wirePipeline;
        wireMaterial = viewer->createMaterial(desc);
    }
    if (wireMaterial != YALN_INVALID_MATERIAL) {
        for (int i = 1; i <= 3; ++i) {
            if (YalnNode* mountain = viewer->scene().root()->find("mountain" + std::to_string(i))) {
                mountain->setMaterialRecursive(wireMaterial);
            }
        }
    }

    std::cout << "Render extension demo: tintMaterial=" << tintMaterial
              << ", wirePipeline=" << wirePipeline
              << ", wireMaterial=" << wireMaterial << std::endl;
}
