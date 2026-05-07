#ifndef GLFWVIEWER_H
#define GLFWVIEWER_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "global_type.h"
#include "yalnview.h"

class GlfwViewer : public YalnView
{
public:
    GlfwViewer(int width = 800, int height = 600, const char* title = "Vulkan App");
    virtual ~GlfwViewer();

    // 运行渲染循环
    void run();

protected:
    // 实现基类的纯虚函数：创建 GLFW 窗口表面
    virtual bool createSurface() override;

    // 获取需要的实例扩展（GLFW 需要）
    virtual std::vector<const char*> getRequiredInstanceExtensions() override;

    // 窗口尺寸改变回调
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

private:
    // 初始化 GLFW
    bool initWindow();

    // 主渲染循环
    void mainLoop();

    // 重新创建交换链（窗口尺寸改变时调用）
    void recreateSwapChain();

    GLFWwindow* _window;
    int _width;
    int _height;
    const char* _title;
};

#endif // GLFWVIEWER_H
