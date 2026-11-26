#include "../include/mainwin.h"

#include "../include/qtviewer.h"
#include "../include/yalnscenefactory.h"
#include "../include/yalnrendermodecontroller.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QElapsedTimer>
#include <QFrame>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QShortcut>
#include <QTimer>
#include <QVBoxLayout>

#include "yalnlight.h"
#include "yalnnode.h"
#include "yalnperspectivecamera.h"
#include "yalnrendertypes.h"

#include <iostream>
#include <string>

YalnMainWin::YalnMainWin(QWidget* parent)
    : QMainWindow(parent)
{
    buildUi();
    connectSignals();

    setWindowTitle("Yaln Vulkan Viewer");
    resize(1280, 800);
}

YalnMainWin::~YalnMainWin()
{
    // 先清空场景树（内部会 waitIdle），断开 drawable 对网格裸指针的引用，
    // 再让 m_sceneAsset 中的网格析构，避免悬空。
    if (m_viewer != nullptr && m_viewer->isInitialized())
    {
        m_viewer->clearMeshes();
    }
}

void YalnMainWin::showStatus(const QString& text)
{
    if (m_statusLabel != nullptr)
    {
        m_statusLabel->setText(text);
    }
}

void YalnMainWin::buildUi()
{
    // ---- 中央区域：Vulkan Viewer + 底部状态栏 ----
    QFrame* centralFrame = new QFrame(this);
    QVBoxLayout* layout = new QVBoxLayout(centralFrame);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_viewer = new QtViewer(centralFrame);
    layout->addWidget(m_viewer, 1);

    m_statusLabel = new QLabel("Ready | FPS: --", centralFrame);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet(
        "QLabel { padding: 4px; background: #2b2b2b; color: #ffffff; }");
    layout->addWidget(m_statusLabel);
    setCentralWidget(centralFrame);

    QMenuBar* menuBar = this->menuBar();

    // ---- 文件菜单 ----
    QMenu* fileMenu = menuBar->addMenu("&File");
    QAction* resetAction = fileMenu->addAction("&Reset View");
    resetAction->setShortcut(QKeySequence("R"));
    fileMenu->addSeparator();
    QAction* exitAction = fileMenu->addAction("E&xit");
    exitAction->setShortcut(QKeySequence("Ctrl+Q"));
    exitAction->setObjectName("exitAction");

    // ---- 视图菜单：自动旋转 + 渲染模式 ----
    QMenu* viewMenu = menuBar->addMenu("&View");
    QAction* toggleRotateAction = viewMenu->addAction("&Auto Rotate");
    toggleRotateAction->setCheckable(true);
    toggleRotateAction->setChecked(true);

    QMenu* modeMenu = viewMenu->addMenu(QString::fromUtf8("渲染模式(&R)"));
    m_modeGroup = new QActionGroup(this);
    m_modeGroup->setExclusive(true);

    struct ModeEntry { YalnRenderMode mode; const char* label; };
    const ModeEntry modes[] = {
        { YalnRenderMode::Normal,        "正常渲染" },
        { YalnRenderMode::WhiteModel,    "白膜渲染" },
        { YalnRenderMode::Wireframe,     "网格渲染" },
        { YalnRenderMode::WhiteAndWire,  "白膜+网格" },
        { YalnRenderMode::NormalAndWire, "正常+网格" },
    };
    for (const ModeEntry& entry : modes)
    {
        QAction* action = modeMenu->addAction(QString::fromUtf8(entry.label));
        action->setCheckable(true);
        action->setChecked(entry.mode == YalnRenderMode::Normal);
        action->setData(static_cast<int>(entry.mode));
        m_modeGroup->addAction(action);
    }

    // ---- 场景菜单 ----
    QMenu* sceneMenu = menuBar->addMenu(QString::fromUtf8("场景(&S)"));
    QAction* loadAction = sceneMenu->addAction(QString::fromUtf8("加载村庄场景"));
    loadAction->setShortcut(QKeySequence("Ctrl+L"));
    loadAction->setObjectName("loadSceneAction");
    QAction* demoAction = sceneMenu->addAction(QString::fromUtf8("应用材质/管线示例"));
    demoAction->setObjectName("demoAction");

    // ---- 帮助菜单 ----
    QMenu* helpMenu = menuBar->addMenu("&Help");
    QAction* aboutAction = helpMenu->addAction("&About");
    aboutAction->setObjectName("aboutAction");

    resetAction->setObjectName("resetAction");
    toggleRotateAction->setObjectName("toggleRotateAction");
}

void YalnMainWin::connectSignals()
{
    QAction* resetAction = findChild<QAction*>("resetAction");
    QAction* exitAction = findChild<QAction*>("exitAction");
    QAction* loadAction = findChild<QAction*>("loadSceneAction");
    QAction* demoAction = findChild<QAction*>("demoAction");
    QAction* aboutAction = findChild<QAction*>("aboutAction");
    QAction* toggleRotateAction = findChild<QAction*>("toggleRotateAction");

    connect(resetAction, &QAction::triggered, m_viewer, &QtViewer::resetTransform);
    connect(toggleRotateAction, &QAction::toggled,
            m_viewer, &QtViewer::setAutoRotate);

    connect(loadAction, &QAction::triggered, this, &YalnMainWin::onLoadVillage);
    connect(m_modeGroup, &QActionGroup::triggered,
            this, &YalnMainWin::onRenderModeTriggered);

    connect(demoAction, &QAction::triggered, [this]() {
        if (!m_viewer->isInitialized() || m_viewer->scene().isEmpty())
        {
            showStatus("请先加载场景");
            return;
        }
        applyRenderExtensionDemo();
        // demo 改写了材质，刷新原始材质快照
        if (m_modeController)
        {
            m_modeController->refreshScene();
        }
        showStatus(QString::fromUtf8("已应用材质/管线示例"));
    });

    connect(exitAction, &QAction::triggered, this, &YalnMainWin::onQuit);
    connect(aboutAction, &QAction::triggered, this, &YalnMainWin::onAbout);

    // ---- ESC 退出 ----
    // QtViewer 的 keyPressEvent 会 accept 按键事件，导致 QShortcut 收不到，
    // 因此以 QtViewer 发出的 escapePressed 信号为主（覆盖 viewer 有焦点的情况），
    // QShortcut 作为 viewer 无焦点时的兜底。
    connect(m_viewer, &QtViewer::escapePressed, this, &YalnMainWin::onQuit);
    QShortcut* escShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    escShortcut->setContext(Qt::ApplicationShortcut);
    connect(escShortcut, &QShortcut::activated, this, &YalnMainWin::onQuit);

    // ---- FPS / 状态信号 ----
    m_fpsTimer.start();
    connect(m_viewer, &QtViewer::frameRendered, this,
            [this](int frameCount) {
                if (m_fpsTimer.elapsed() >= 1000)
                {
                    int fps = frameCount - m_lastFrameCount;
                    m_lastFrameCount = frameCount;
                    m_fpsTimer.restart();
                    showStatus(QString("Frame: %1 | FPS: %2")
                                   .arg(frameCount).arg(fps));
                }
            });

    connect(m_viewer, &QtViewer::vulkanInitialized, this, [this](bool success) {
        showStatus(success ? QString::fromUtf8("Vulkan 初始化成功")
                           : QString::fromUtf8("Vulkan 初始化失败"));
    });
    connect(m_viewer, &QtViewer::errorOccurred, this, [this](const QString& error) {
        showStatus(QString("Error: %1").arg(error));
    });
}

void YalnMainWin::onQuit()
{
    close();
}

void YalnMainWin::onAbout()
{
    QMessageBox::about(this, "About Yaln Vulkan Viewer",
        "<h3>Yaln Vulkan Viewer</h3>"
        "<p>Qt Widget-based Vulkan renderer.</p>"
        "<p><b>Controls:</b><br>"
        "- Left Mouse: Orbit camera<br>"
        "- Right Mouse: Pan camera<br>"
        "- Middle Mouse / Scroll: Zoom<br>"
        "- WASD: Move camera / QE: Up-Down<br>"
        "- R: Reset view<br>"
        "- ESC: Exit</p>"
        "<p><b>Render modes</b> (View menu): Normal / WhiteModel / Wireframe / "
        "WhiteAndWire / NormalAndWire</p>");
}

void YalnMainWin::onLoadVillage()
{
    if (!m_viewer->isInitialized())
    {
        showStatus("Vulkan 未初始化");
        return;
    }

    m_viewer->device().waitIdle();

    YalnSceneAsset asset = YalnSceneFactory::createVillage();
    if (!asset.root)
    {
        showStatus("场景构建失败");
        return;
    }

    m_viewer->scene().clear();
    m_viewer->scene().setRoot(std::move(asset.root));
    for (auto& light : asset.lights)
    {
        m_viewer->scene().addLight(std::move(light));
    }
    m_sceneAsset = std::move(asset);

    if (!m_viewer->rebuildScene())
    {
        showStatus(QString::fromUtf8("场景 GPU 缓冲构建失败"));
        return;
    }

    // 模式控制器重新采集 drawable 与原始材质
    if (m_modeController)
    {
        m_modeController->refreshScene();
        // 保持当前选中的模式（重新应用到新场景）
        m_modeController->setMode(m_modeController->mode());
    }

    showStatus(QString::fromUtf8("场景已加载"));
}

void YalnMainWin::onRenderModeTriggered(QAction* action)
{
    if (action == nullptr || !m_modeController)
    {
        return;
    }

    bool ok = false;
    const int raw = action->data().toInt(&ok);
    if (!ok)
    {
        return;
    }

    const YalnRenderMode mode = static_cast<YalnRenderMode>(raw);
    if (m_modeController->setMode(mode))
    {
        showStatus(QString::fromUtf8("渲染模式: %1")
                       .arg(QString::fromStdString(std::string(m_modeController->modeName()))));
    }
    else
    {
        // 切换失败：把勾选还原到当前实际模式
        for (QAction* a : m_modeGroup->actions())
        {
            if (a->data().toInt() == static_cast<int>(m_modeController->mode()))
            {
                a->setChecked(true);
                break;
            }
        }
    }
}

void YalnMainWin::applyRenderExtensionDemo()
{
    // 兼容旧的 --demo-render-extension：风车暖色（同管线不同材质）
    static YalnMaterialHandle tintMaterial = YALN_INVALID_MATERIAL;
    if (tintMaterial == YALN_INVALID_MATERIAL)
    {
        YalnMaterialDesc desc;
        desc.name = "WindmillTint";
        desc.baseColor = glm::vec4(1.0f, 0.78f, 0.42f, 1.0f);
        desc.defaultPipeline = YALN_DEFAULT_PIPELINE;
        tintMaterial = m_viewer->createMaterial(desc);
    }
    if (YalnNode* windmill = m_viewer->scene().root()->find("windmill"))
    {
        windmill->setMaterialRecursive(tintMaterial);
    }
    std::cout << "[Demo] tintMaterial=" << tintMaterial.value() << std::endl;
}

bool YalnMainWin::initializeVulkan(std::string_view appName)
{
    show();

    // ---- 渲染器配置（必须在 initialize 之前注入）----
    YalnRenderPassConfig rpc = YalnRenderPassConfig::makeDefault();
    rpc.enableDepth = true;
    m_viewer->setRenderPassConfig(rpc);

    YalnPipelineConfig ppc = YalnPipelineConfig::makeDefault();
    ppc.depthTest = true;
    ppc.depthWrite = true;
    m_viewer->setPipelineConfig(ppc);

    YalnRendererConfig rc = YalnRendererConfig::makeDefault();
    rc.clearColor = { 0.53f, 0.81f, 0.92f, 1.0f };  // 天空蓝
    m_viewer->setRendererConfig(rc);

    if (!m_viewer->initialize(appName))
    {
        QMessageBox::critical(this, "Error", QString("Failed to initialize Vulkan!\n\n")
                              + QString::fromStdString(m_viewer->lastError()));
        return false;
    }

    // ---- 相机 ----
    const float aspect = 1280.0f / 800.0f;
    YalnPerspectiveCameraPtr camera(
        new YalnPerspectiveCamera(45.0f, aspect, 0.1f, 100.0f));
    camera->setPosition(glm::vec3(16.0f, -18.0f, 14.0f));
    camera->setTarget(glm::vec3(0.0f, 0.0f, 1.0f));
    m_viewer->setCameta(camera);

    // ---- 模式控制器（初始化后创建，才能注册线框 PSO）----
    m_modeController = std::make_unique<YalnRenderModeController>(*m_viewer);

    // ---- 命令行：自动加载场景 / 直接指定模式 ----
    const QStringList args = QApplication::arguments();
    if (args.contains("--demo-render-extension"))
    {
        onLoadVillage();
        if (!m_viewer->scene().isEmpty())
        {
            applyRenderExtensionDemo();
            m_modeController->refreshScene();
        }
    }
    else if (args.contains("--load-village"))
    {
        onLoadVillage();
    }

    // --mode=0..4：Normal / WhiteModel / Wireframe / WhiteAndWire / NormalAndWire
    for (const QString& arg : args)
    {
        if (arg.startsWith("--mode="))
        {
            bool ok = false;
            int modeIndex = arg.mid(7).toInt(&ok);
            if (ok && modeIndex >= 0 && modeIndex <= 4)
            {
                const YalnRenderMode mode = static_cast<YalnRenderMode>(modeIndex);
                m_modeController->setMode(mode);
                for (QAction* a : m_modeGroup->actions())
                {
                    if (a->data().toInt() == modeIndex)
                    {
                        a->setChecked(true);
                        break;
                    }
                }
            }
            break;
        }
    }

    // --quit-after=秒：自动化/CI 用，调用与 ESC 相同的 onQuit() 关闭流程
    for (const QString& arg : args)
    {
        if (arg.startsWith("--quit-after="))
        {
            bool ok = false;
            int seconds = arg.mid(13).toInt(&ok);
            if (ok && seconds > 0)
            {
                QTimer::singleShot(seconds * 1000, this, &YalnMainWin::onQuit);
            }
            break;
        }
    }

    m_viewer->startRenderLoop();
    return true;
}
