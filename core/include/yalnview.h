#ifndef YALN_VIEW_H
#define YALN_VIEW_H

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <string>
#include <vector>

#include "global_type.h"

#include "yalncamera.h"
#include "yalnmesh.h"

// 组合的各个子系统（每个只负责一件事）
#include "yalnvulkaninstance.h"
#include "yalnsurface.h"
#include "yalndevice.h"
#include "yalnswapchain.h"
#include "yalnrenderpass.h"
#include "yalngraphicspipeline.h"
#include "yalnscene.h"
#include "yalnrenderer.h"

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = YALN_MAX_FRAMES_IN_FLIGHT;

// ============================================================================
//  YalnView
//  职责（重构后）：仅做「编排 + 对外门面」。
//    - 按顺序驱动各子系统初始化
//    - 向上层（QtViewer / main）暴露简洁 API
//  实例、设备、交换链、渲染通道、管线、场景、渲染循环分别由
//  YalnVulkanInstance / YalnDevice / YalnSwapChain / YalnRenderPass /
//  YalnGraphicsPipeline / YalnScene / YalnRenderer 承担。
// ============================================================================
class YALN_CORE_EXPORT YalnView
{
public:
    YalnView();
    virtual ~YalnView();

    YalnView(const YalnView&)            = delete;
    YalnView& operator=(const YalnView&) = delete;

    // 按顺序初始化全部 Vulkan 子系统
    bool initVulkan(const char* appName);

    void printCapacity();

    // 相机（保留旧拼写 setCameta 以兼容现有调用）
    void          setCameta(YalnCameraPtr camera);
    YalnCameraPtr camera() const          { return m_camera_ptr; }
    YalnCameraPtr getCamera() const       { return m_camera_ptr; }

    // 场景
    void addMesh(YalnMesh* mesh);
    void clearMeshes();
    bool isMeshReady() const;

    // 着色器路径（默认为运行目录下的 ../shaders/*.spv）
    void setShaderPaths(const std::string& vertPath, const std::string& fragPath)
    {
        m_vertShaderPath = vertPath;
        m_fragShaderPath = fragPath;
    }

    // 子系统访问（供平台层创建表面 / 查询尺寸）
    const YalnVulkanInstance& vulkanInstance() const { return m_instance; }
    YalnSurface&              surface()              { return m_surface; }
    const YalnDevice&         device()         const { return m_device; }
    const YalnSwapChain&      swapChain()      const { return m_swapChain; }
    vk::Extent2D              swapChainExtent() const { return m_swapChain.extent(); }

    bool isInitialized() const { return m_initialized; }

protected:
    // 由平台层实现：使用自己的窗口句柄创建表面并写入 surface()
    virtual bool createSurface() = 0;

    // 实例级扩展（默认含 surface 扩展与校验层调试扩展）
    virtual std::vector<const char*> getRequiredInstanceExtensions();

    // 帧循环与交换链重建（由平台层在定时器/尺寸变化时驱动）
    virtual void recreateSwapChain();
    void         drawFrame();

private:
    bool createVulkanInstance(const char* appName);
    void createMeshBuffers();

    // ------------------------------------------------------------------
    // 成员声明顺序 = 创建顺序；析构逆序，保证 instance 最后销毁。
    // ------------------------------------------------------------------
    YalnVulkanInstance  m_instance;
    YalnSurface         m_surface;
    YalnDevice          m_device;
    YalnSwapChain       m_swapChain;
    YalnRenderPass      m_renderPass;
    YalnGraphicsPipeline m_pipeline;
    YalnRenderer        m_renderer;
    YalnScene           m_scene;

    YalnCameraPtr m_camera_ptr = nullptr;
    bool          m_initialized = false;

    std::string m_vertShaderPath = "../shaders/shader.vert.spv";
    std::string m_fragShaderPath = "../shaders/shader.frag.spv";
};

#endif // YALN_VIEW_H
