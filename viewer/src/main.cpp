// Yaln Vulkan Viewer 入口
// 窗口/菜单/场景/渲染模式逻辑全部收敛到 YalnMainWin，这里只做最小装配。

#include <QApplication>

#include "mainwin.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Yaln Vulkan Viewer");
    app.setApplicationVersion("1.1.0");
    app.setOrganizationName("YalnEngine");

    YalnMainWin window;
    if (!window.initializeVulkan("Yaln Vulkan Viewer"))
    {
        return 1;
    }

    return app.exec();
}
