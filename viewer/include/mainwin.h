#ifndef YALN_MAIN_WIN_H
#define YALN_MAIN_WIN_H

#include <QMainWindow>
#include <QElapsedTimer>
#include <memory>
#include <string_view>

#include "yalnscenefactory.h"
#include "yalnrendermodecontroller.h"

class QLabel;
class QtViewer;
class QActionGroup;

// ============================================================================
//  YalnMainWin —— Viewer 的主窗口
//
//  把窗口/菜单/状态栏/信号连接/相机配置/Vulkan 初始化/场景加载/渲染模式切换
//  全部收敛到此类，main() 只负责创建 QApplication 和 YalnMainWin。
// ============================================================================
class YalnMainWin : public QMainWindow
{
    Q_OBJECT

public:
    explicit YalnMainWin(QWidget* parent = nullptr);
    ~YalnMainWin() override;

    // 完成渲染器配置、Vulkan 初始化、相机设置、可选命令行场景加载，并启动渲染循环。
    bool initializeVulkan(std::string_view appName = "Yaln Vulkan Viewer");

private slots:
    void onLoadVillage();
    void onRenderModeTriggered(QAction* action);
    void onAbout();
    void onQuit();

private:
    void buildUi();
    void connectSignals();
    void applyRenderExtensionDemo();   // 兼容旧的 --demo-render-extension 演示
    void showStatus(const QString& text);

private:
    QtViewer*  m_viewer = nullptr;
    QLabel*    m_statusLabel = nullptr;
    QActionGroup* m_modeGroup = nullptr;

    YalnSceneAsset m_sceneAsset;
    std::unique_ptr<YalnRenderModeController> m_modeController;

    QElapsedTimer m_fpsTimer;
    int           m_lastFrameCount = 0;
};

#endif // YALN_MAIN_WIN_H
