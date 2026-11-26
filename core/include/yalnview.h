#ifndef YALN_VIEW_H
#define YALN_VIEW_H

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "global_type.h"

#include "yalncamera.h"
#include "yalnmesh.h"
#include "yalninterfaces.h"
#include "yalnshaderprogram.h"
#include "yalnmateriallibrary.h"
#include "yalnpipelinelibrary.h"

class IYalnFramePipeline;

// 组合的各个子系统：接口 + 默认实现（默认实现可整体替换）
#include "yalnvulkaninstance.h"
#include "yalnsurface.h"
#include "yalndevice.h"
#include "yalnswapchain.h"
#include "yalnrenderpass.h"
#include "yalngraphicspipeline.h"
#include "yalnscene.h"
#include "yalnrenderer.h"

// ============================================================================
//  YalnView
//  职责：仅做「编排 + 对外门面」。
//    - 按顺序驱动各子系统初始化（不变的流程）；
//    - 子系统以 IYalnXxx 接口指针持有，默认实现可整体替换（继承扩展）；
//    - 各子系统行为可通过 XxxConfig 结构体定制（配置扩展）；
//    - 初始化前做组合合理性校验（validateConfig）。
//
//  替换实现：setInstance/setSurface/... 必须在 initVulkan 之前调用。
// ============================================================================
class YALN_CORE_EXPORT YalnView
{
public:
    YalnView();
    virtual ~YalnView();

    YalnView(const YalnView&)            = delete;
    YalnView& operator=(const YalnView&) = delete;

    // 按顺序初始化全部 Vulkan 子系统（失败时查看 lastError()）
    bool initVulkan(std::string_view appName);

    void printCapacity();

    // ==================== 实现替换（继承扩展）====================
    // 注入自定义实现，接管所有权；仅可在 initVulkan 之前调用。
    // 传入 nullptr 会被忽略并保持默认实现。
    bool setInstance(std::unique_ptr<IYalnInstance> inst);
    bool setSurface(std::unique_ptr<IYalnSurface> surf);
    bool setDevice(std::unique_ptr<IYalnDevice> dev);
    bool setSwapChain(std::unique_ptr<IYalnSwapChain> swapChain);
    bool setRenderPass(std::unique_ptr<IYalnRenderPass> renderPass);
    bool setShaderProgram(std::unique_ptr<IYalnShaderProgram> program);
    bool setPipeline(std::unique_ptr<IYalnPipeline> pipeline);
    bool setRenderer(std::unique_ptr<IYalnRenderer> renderer);
    bool setScene(std::unique_ptr<IYalnScene> scene);

    // 帧级策略：初始化前保存，初始化时转交渲染器；初始化后直接热替换。
    bool setFramePipeline(std::unique_ptr<IYalnFramePipeline> pipeline);

    // ==================== 配置注入（配置扩展）====================
    void setRenderPassConfig(const YalnRenderPassConfig& config) { m_renderPassConfig = config; }
    void setPipelineConfig(const YalnPipelineConfig&    config) { m_pipelineConfig  = config; }
    void setRendererConfig(const YalnRendererConfig&    config) { m_rendererConfig  = config; }

    // 着色器路径（写进管线配置；默认为运行目录下的 ../shaders/*.spv）
    void setShaderPaths(const std::string& vertPath, const std::string& fragPath)
    {
        m_pipelineConfig.vertShaderPath = vertPath;
        m_pipelineConfig.fragShaderPath = fragPath;
    }

    // 相机（保留旧拼写 setCameta 以兼容现有调用）
    void          setCameta(YalnCameraPtr camera);
    YalnCameraPtr camera() const    { return m_camera_ptr; }
    YalnCameraPtr getCamera() const { return m_camera_ptr; }

    // 场景
    void addMesh(YalnMesh* mesh);
    void clearMeshes();
    bool isMeshReady() const;

    // 手动重建场景 GPU 缓冲（直接操作场景树后调用；addMesh 路径会自动重建）
    bool rebuildScene();

    // ==================== 子系统访问 ====================
    // 默认实现特有方法（如 YalnVulkanInstance::validationLayerEnabled）
    // 可通过具体类型访问：static_cast<const YalnVulkanInstance&>(vulkanInstance())
    const IYalnInstance&  vulkanInstance() const { return *m_instance; }
    IYalnSurface&         surface()        const { return *m_surface; }
    const IYalnDevice&    device()         const { return *m_device; }
    const IYalnSwapChain& swapChain()      const { return *m_swapChain; }
    const IYalnRenderPass& renderPass()    const { return *m_renderPass; }
    const IYalnPipeline&  pipeline()       const { return *m_pipeline; }
    IYalnRenderer&        renderer()       const { return *m_renderer; }
    IYalnScene&           scene()          const { return *m_scene; }

    IYalnShaderProgram* shaderProgram() const { return m_shaderProgram.get(); }
    IYalnFramePipeline* framePipeline() const;

    // 资源库（初始化后可用）：注册额外 PSO、创建材质
    YalnPipelineLibrary* pipelineLibrary() const;
    YalnMaterialLibrary* materialLibrary() const;

    // 便捷扩展：使用当前着色器程序创建一条新 PSO 并登记。
    // 需要不同着色器时，自行创建 IYalnPipeline 后调用 pipelineLibrary()->addPipeline()。
    YalnPipelineHandle createPipeline(const YalnPipelineConfig& config,
                                      const std::string&        name = {});

    // 使用调用方提供的着色器程序创建管线（例如白膜/线框等需要不同 shader 的 PSO）。
    // shaderProgram 必须已 load()；管线只引用它，不接管所有权。
    YalnPipelineHandle createPipeline(const YalnPipelineConfig& config,
                                      const IYalnShaderProgram& shaderProgram,
                                      const std::string&        name = {});

    // 便捷扩展：创建材质（句柄用于 YalnDrawable::setMaterial）
    YalnMaterialHandle createMaterial(const YalnMaterialDesc& desc);

    vk::Extent2D swapChainExtent() const { return m_swapChain->extent(); }

    bool isInitialized() const { return m_initialized; }

    // 初始化失败时的人类可读错误（组合校验结果 / 底层异常信息）
    const std::string& lastError() const { return m_lastError; }

protected:
    virtual bool init() { return true; }

    // 由平台层实现：使用自己的窗口句柄创建表面并写入 surface()
    virtual bool createSurface() = 0;

    // 实例级扩展（默认含 surface 扩展与校验层调试扩展）
    virtual std::vector<const char*> getRequiredInstanceExtensions();

    // 帧循环与交换链重建（由平台层在定时器/尺寸变化时驱动）
    virtual void recreateSwapChain();
    void         drawFrame();

private:
    bool createVulkanInstance(std::string_view appName);
    void createMeshBuffers();

    // 组合合理性校验：依赖设备/交换链信息，在两者创建后、子系统创建前调用。
    // 返回 false 时 issues 中给出全部问题（而非首个即停）。
    bool validateConfig(std::vector<std::string>& issues);

    // ------------------------------------------------------------------
    // 成员声明顺序 = 创建顺序；析构逆序，保证 instance 最后销毁。
    // 接口指针默认指向默认实现，可在 initVulkan 前整体替换。
    // ------------------------------------------------------------------
    std::unique_ptr<IYalnInstance>   m_instance    = std::make_unique<YalnVulkanInstance>();
    std::unique_ptr<IYalnSurface>    m_surface     = std::make_unique<YalnSurface>();
    std::unique_ptr<IYalnDevice>     m_device      = std::make_unique<YalnDevice>();
    std::unique_ptr<IYalnSwapChain>  m_swapChain   = std::make_unique<YalnSwapChain>();
    std::unique_ptr<IYalnRenderPass> m_renderPass  = std::make_unique<YalnRenderPass>();

    // 可选着色器程序：nullptr 时管线按 m_pipelineConfig 的路径内建
    std::unique_ptr<IYalnShaderProgram> m_shaderProgram;

    std::unique_ptr<IYalnPipeline>   m_pipeline    = std::make_unique<YalnGraphicsPipeline>();
    std::unique_ptr<IYalnFramePipeline> m_framePipeline;
    std::unique_ptr<IYalnRenderer>   m_renderer    = std::make_unique<YalnRenderer>();
    std::unique_ptr<IYalnScene>      m_scene       = std::make_unique<YalnScene>();

    // 可插拔配置（默认值与原行为完全一致）
    YalnRenderPassConfig m_renderPassConfig = YalnRenderPassConfig::makeDefault();
    YalnPipelineConfig   m_pipelineConfig   = YalnPipelineConfig::makeDefault();
    YalnRendererConfig   m_rendererConfig   = YalnRendererConfig::makeDefault();

    YalnCameraPtr m_camera_ptr = nullptr;
    bool          m_initialized = false;

    std::string m_lastError;
};

#endif // YALN_VIEW_H
