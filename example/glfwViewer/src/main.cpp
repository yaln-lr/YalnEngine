#include "../include/glfwviewer.h"

#include <iostream>

int main()
{
    try
    {
        // 创建 GlfwViewer 实例，窗口大小 800x600，标题为 "GlfwViewer Test"
        GlfwViewer viewer(800, 600, "GlfwViewer Test");
        // 运行渲染循环
        viewer.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
