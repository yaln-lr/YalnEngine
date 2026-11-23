#ifndef YALN_INTERFACES_H
#define YALN_INTERFACES_H

// ============================================================================
//  YalnEngine 核心接口契约
//
//  分层模式（与相机/网格一致）：
//      IYalnXxx    纯虚接口（零状态零实现）——上层只依赖它
//          ▲
//      YalnXxx     默认实现（Vulkan 后端）——可整体替换
//
//  扩展方式（二选一或组合）：
//      1. 继承扩展：实现 IYalnXxx（或派生默认实现重写个别方法），
//         通过 YalnView::setXxx() 注入；
//      2. 配置扩展：保持默认实现，通过 YalnXxxConfig 结构体定制行为。
//
//  封装边界：
//      接口方法签名只暴露「值类型」（vk::Format / vk::Extent2D / 句柄）
//      与配置结构体；创建逻辑、依赖顺序、资源生命周期全部封装在实现内部。
// ============================================================================

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <glm/glm.hpp>

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "global_type.h"
#include "yalncamera.h"
#include "yalnbuffer.h"   // YalnGpuMesh 需要缓冲类型
#include "yalnlight.h"    // 场景级光源
#include "yalnrendertypes.h"

class IYalnDevice;
class IYalnSwapChain;
class YalnMesh;

// 帧级策略与资源库（定义在各自头文件；接口层只前向声明）
class IYalnFramePipeline;
class YalnPipelineLibrary;
class YalnMaterialLibrary;

// ============================================================================
//  共享值类型
// ============================================================================

// 队列族索引（原 YalnDevice 内嵌结构，提升为共享类型供接口使用）
struct YALN_CORE_EXPORT YalnQueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    bool isComplete() const {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

// 交换链支持详情
struct YALN_CORE_EXPORT YalnSwapChainSupportDetails {
    vk::SurfaceCapabilitiesKHR        capabilities;
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR>   presentModes;
};

// 帧循环结果（原 YalnRenderer::FrameResult，兼容别名见 yalnrenderer.h）
enum class YalnFrameResult {
    Rendered,            // 正常出帧
    SwapChainOutOfDate,  // 需要重建交换链
    NotReady,            // 场景尚未就绪，跳过
    Failed               // 提交/获取失败
};

// ============================================================================
//  配置契约（配置扩展的载体；默认值 = 默认实现的原始行为）
// ============================================================================

// ---- 渲染通道配置 ----
struct YALN_CORE_EXPORT YalnRenderPassConfig {
    bool enableDepth = false;  // 深度附件随帧缓冲自动分配/重建
    vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1;

    vk::AttachmentLoadOp  colorLoadOp      = vk::AttachmentLoadOp::eClear;
    vk::AttachmentStoreOp colorStoreOp     = vk::AttachmentStoreOp::eStore;
    vk::ImageLayout       colorFinalLayout = vk::ImageLayout::ePresentSrcKHR;

    vk::AttachmentLoadOp  depthLoadOp      = vk::AttachmentLoadOp::eClear;
    vk::AttachmentStoreOp depthStoreOp     = vk::AttachmentStoreOp::eDontCare;

    static YalnRenderPassConfig makeDefault() { return {}; }
};

// ---- 图形管线配置 ----
// 约束：depthTest/depthWrite 要求渲染通道启用深度附件；samples 须与渲染通道一致
struct YALN_CORE_EXPORT YalnPipelineConfig {
    std::string vertShaderPath;
    std::string fragShaderPath;

    vk::PrimitiveTopology topology    = vk::PrimitiveTopology::eTriangleList;
    vk::PolygonMode       polygonMode = vk::PolygonMode::eFill;
    vk::CullModeFlags     cullMode    = vk::CullModeFlagBits::eBack;
    vk::FrontFace         frontFace   = vk::FrontFace::eCounterClockwise;
    float                 lineWidth   = 1.0f;

    bool          depthTest      = false;
    bool          depthWrite     = false;
    vk::CompareOp depthCompareOp = vk::CompareOp::eLessOrEqual;

    bool blendEnable = false;
    vk::BlendFactor blendSrcColorFactor = vk::BlendFactor::eSrcAlpha;
    vk::BlendFactor blendDstColorFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    vk::BlendFactor blendSrcAlphaFactor = vk::BlendFactor::eOne;
    vk::BlendFactor blendDstAlphaFactor = vk::BlendFactor::eZero;

    std::vector<vk::DescriptorSetLayoutBinding> descriptorBindings;  // 空 = 无描述符

    // 自定义顶点布局；为空时默认使用 YalnVertexPC（位置 + 颜色）。
    // 典型用法：新网格格式 = 新顶点结构 + 这里填 binding/attribute。
    std::vector<vk::VertexInputBindingDescription>   vertexBindings;
    std::vector<vk::VertexInputAttributeDescription> vertexAttributes;

    // 默认与 shader.vert/frag 的 push_constant 块一致（world + baseColor）
    vk::PushConstantRange pushConstant =
        vk::PushConstantRange(vk::ShaderStageFlagBits::eVertex |
                              vk::ShaderStageFlagBits::eFragment,
                              0, sizeof(YalnMaterialPushConstants));

    vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1;

    static YalnPipelineConfig makeDefault();
};

// ---- 渲染器配置 ----
// 约束：maxFramesInFlight ∈ [1, 交换链图像数量]；uniformBinding 须存在于管线描述符绑定
struct YALN_CORE_EXPORT YalnRendererConfig {
    uint32_t maxFramesInFlight = 2;
    uint32_t uniformBinding    = 0;
    // 清除色（渲染窗口背景）：天空蓝——空场景与已加载场景共用同一片"天空"
    std::array<float, 4> clearColor = { 0.53f, 0.78f, 0.92f, 1.0f };

    static YalnRendererConfig makeDefault() { return {}; }
};

// ---- 着色器程序配置（工厂参数）----
struct YALN_CORE_EXPORT YalnShaderProgramConfig {
    std::string vertPath;
    std::string fragPath;
    std::string entryPoint = "main";
};

// ============================================================================
//  单个网格的 GPU 资源（原 yalnscene.h 内定义，提升为共享类型）
// ============================================================================
struct YALN_CORE_EXPORT YalnGpuMesh {
    YalnBuffer vertexBuffer;
    YalnBuffer indexBuffer;
    uint32_t   indexCount = 0;
};

// 一次 draw call 的全部信息：GPU 缓冲 + 世界矩阵 + 材质/Pass 选择
// （同一网格多处实例共享同一份 GPU 缓冲；由场景 update() 收集生成）
struct YALN_CORE_EXPORT YalnDrawItem {
    const YalnGpuMesh* gpuMesh = nullptr;
    glm::mat4          world{ 1.0f };

    // 材质句柄：默认 0；材质决定使用哪条管线、哪种材质参数
    YalnMaterialHandle material = YALN_DEFAULT_MATERIAL;

    // 该绘制项参与的 Pass 位掩码（默认全部 Pass）
    uint32_t passMask = YALN_PASS_MASK_ALL;

    // 可选：直接覆盖材质中的管线选择（调试/特殊对象用，默认无）
    YalnPipelineHandle pipelineOverride = YALN_INVALID_PIPELINE;

    // 调试/剔除用：源网格与世界空间包围盒
    const YalnMesh* mesh = nullptr;
    glm::vec3       boundsMin{ 0.0f };
    glm::vec3       boundsMax{ 0.0f };
};

// ============================================================================
//  IYalnInstance —— Vulkan 实例（创建顺序第 1 位）
// ============================================================================
class YALN_CORE_EXPORT IYalnInstance
{
public:
    virtual ~IYalnInstance() = default;

    virtual bool create(const char* applicationName, const std::vector<const char*>& extensions) = 0;
    virtual void destroy() {}

    virtual const vk::raii::Context&  context() const = 0;
    virtual const vk::raii::Instance& handle()  const = 0;
    virtual bool isValid() const = 0;
};

// ============================================================================
//  IYalnSurface —— 窗口表面（第 2 位；由平台层注入创建好的表面）
// ============================================================================
class YALN_CORE_EXPORT IYalnSurface
{
public:
    virtual ~IYalnSurface() = default;

    // 平台层创建表面后交接到此处接管生命周期
    virtual void set(vk::raii::SurfaceKHR&& surface) = 0;
    virtual void destroy() = 0;

    virtual const vk::raii::SurfaceKHR& handle() const = 0;
    // C API 查询（如 vkGetPhysicalDeviceSurfaceSupportKHR）使用裸句柄
    virtual VkSurfaceKHR rawHandle() const = 0;
    virtual bool isValid() const = 0;
};

// ============================================================================
//  IYalnDevice —— 物理设备挑选 + 逻辑设备 + 队列（第 3 位）
// ============================================================================
class YALN_CORE_EXPORT IYalnDevice
{
public:
    virtual ~IYalnDevice() = default;

    virtual bool initialize(const IYalnInstance& instance, const IYalnSurface& surface) = 0;
    virtual void waitIdle() const = 0;

    virtual const vk::raii::PhysicalDevice& physicalDevice() const = 0;
    virtual const vk::raii::Device&         device()         const = 0;
    virtual const vk::raii::Queue&          graphicsQueue()  const = 0;
    virtual const vk::raii::Queue&          presentQueue()   const = 0;
    virtual const YalnQueueFamilyIndices&   queueFamilies()  const = 0;

    virtual YalnSwapChainSupportDetails
        querySwapChainSupport(const vk::raii::PhysicalDevice& physicalDevice) const = 0;

    virtual uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const = 0;

    virtual bool isValid() const = 0;
};

// ============================================================================
//  IYalnSwapChain —— 交换链 + 图像 + 视图（第 4 位；重建时仅销毁重建自身）
// ============================================================================
class YALN_CORE_EXPORT IYalnSwapChain
{
public:
    virtual ~IYalnSwapChain() = default;

    virtual bool create(const IYalnDevice& device, const IYalnSurface& surface) = 0;
    virtual void destroy() = 0;

    virtual const vk::raii::SwapchainKHR&   handle()     const = 0;
    virtual const std::vector<vk::Image>&   images()     const = 0;
    virtual const std::vector<vk::raii::ImageView>& imageViews() const = 0;
    virtual vk::Format   format() const = 0;
    virtual vk::Extent2D extent() const = 0;
    virtual bool isValid() const = 0;
};

// ============================================================================
//  IYalnRenderPass —— 渲染通道 + 帧缓冲（第 5 位；帧缓冲随交换链重建）
// ============================================================================
class YALN_CORE_EXPORT IYalnRenderPass
{
public:
    virtual ~IYalnRenderPass() = default;

    virtual bool create(const IYalnDevice&    device,
                        vk::Format            colorFormat,
                        const YalnRenderPassConfig& config = YalnRenderPassConfig::makeDefault()) = 0;
    virtual bool createFramebuffers(const IYalnDevice& device, const IYalnSwapChain& swapChain) = 0;

    virtual void destroyFramebuffers() = 0;
    virtual void destroy() = 0;

    virtual const vk::raii::RenderPass&               handle()       const = 0;
    virtual const std::vector<vk::raii::Framebuffer>& framebuffers() const = 0;
    virtual bool hasDepth() const = 0;
    virtual vk::SampleCountFlagBits samples() const = 0;
    virtual bool isValid() const = 0;
};

// ============================================================================
//  IYalnShader —— 单个着色器的抽象
//
//  每个 YalnShader 实例封装「一个着色器阶段」：它是什么阶段、入口点叫什么、
//  从哪里加载、模块生命周期归谁管。想支持哪个阶段就派生/添加哪个阶段的实现。
//
//  注意：stageInfo() 返回的信息引用已加载的模块，
//        在管线 create 期间着色器对象必须保持存活。
// ============================================================================
enum class YalnShaderStage {
    Vertex, Fragment, TessControl, TessEvaluation, Geometry, Compute
};

inline vk::ShaderStageFlagBits toVulkanShaderStage(YalnShaderStage stage)
{
    switch (stage)
    {
        case YalnShaderStage::Vertex:         return vk::ShaderStageFlagBits::eVertex;
        case YalnShaderStage::Fragment:       return vk::ShaderStageFlagBits::eFragment;
        case YalnShaderStage::TessControl:    return vk::ShaderStageFlagBits::eTessellationControl;
        case YalnShaderStage::TessEvaluation: return vk::ShaderStageFlagBits::eTessellationEvaluation;
        case YalnShaderStage::Geometry:       return vk::ShaderStageFlagBits::eGeometry;
        case YalnShaderStage::Compute:        return vk::ShaderStageFlagBits::eCompute;
    }
    return vk::ShaderStageFlagBits::eVertex;
}

inline std::string toString(YalnShaderStage stage)
{
    switch (stage)
    {
        case YalnShaderStage::Vertex:         return "Vertex";
        case YalnShaderStage::Fragment:       return "Fragment";
        case YalnShaderStage::TessControl:    return "TessControl";
        case YalnShaderStage::TessEvaluation: return "TessEvaluation";
        case YalnShaderStage::Geometry:       return "Geometry";
        case YalnShaderStage::Compute:        return "Compute";
    }
    return "Unknown";
}

class YALN_CORE_EXPORT IYalnShader
{
public:
    virtual ~IYalnShader() = default;

    virtual YalnShaderStage stage() const = 0;
    virtual std::string entryPoint() const = 0;

    // 读源数据 + 创建 ShaderModule（幂等：重复调用先卸载）
    virtual bool load(const vk::raii::Device& device) = 0;
    virtual void unload() = 0;
    virtual bool isValid() const = 0;

    virtual vk::PipelineShaderStageCreateInfo stageInfo() const = 0;
};

// ============================================================================
//  IYalnShaderProgram —— 着色器程序（着色器集合 + 组合规则）
//
//  程序 = 一组 IYalnShader。实现哪些阶段由派生/调用方决定，
//  但必须满足以下规则（load() 时校验，违反则失败并给出原因）：
//
//    图形程序：
//      - 必须实现 Vertex 与 Fragment（引擎约定；
//        Vulkan 本身允许无片元的深度 pre-pass，出现该需求时再放宽）
//      - TessControl 与 TessEvaluation 必须成对出现
//      - TessControl/TessEvaluation 需要 tessellationShader 设备特性
//      - Geometry 需要 geometryShader 设备特性
//    计算程序：
//      - 必须实现 Compute，且不得混入图形阶段
//    通用：
//      - 同一阶段不得重复；空程序非法；阶段混入（图形+计算）非法
// ============================================================================
class YALN_CORE_EXPORT IYalnShaderProgram
{
public:
    // 程序类型（按包含的阶段推断，供管线判断能否使用）
    enum class Type { Empty, Graphics, Compute, Mixed };

    virtual ~IYalnShaderProgram() = default;

    // 添加一个着色器（接管所有权）。已加载状态下拒绝添加（需重新 load）。
    virtual bool addShader(std::unique_ptr<IYalnShader> shader) = 0;

    // 加载全部着色器 + 校验组合规则与设备特性（幂等：重复调用先卸载）
    virtual bool load(const IYalnDevice& device) = 0;
    virtual void unload() = 0;

    virtual bool isValid() const = 0;
    virtual Type  type()  const = 0;

    virtual const std::vector<std::unique_ptr<IYalnShader>>& shaders() const = 0;

    // 供管线组装 vk::PipelineShaderStageCreateInfo（顺序无关）
    virtual std::vector<vk::PipelineShaderStageCreateInfo> stageInfos() const = 0;
};

// ============================================================================
//  IYalnPipeline —— 管线布局 + 图形管线（第 6 位；依赖渲染通道与着色器程序）
// ============================================================================
class YALN_CORE_EXPORT IYalnPipeline
{
public:
    virtual ~IYalnPipeline() = default;

    virtual bool create(const IYalnDevice&          device,
                        const vk::raii::RenderPass&  renderPass,
                        const YalnPipelineConfig&    config,
                        const IYalnShaderProgram&    shaderProgram) = 0;
    virtual void destroy() = 0;

    virtual const vk::raii::DescriptorSetLayout& descriptorSetLayout() const = 0;
    virtual const vk::raii::PipelineLayout&      layout()             const = 0;
    virtual const vk::raii::Pipeline&            handle()             const = 0;
    virtual const YalnPipelineConfig&            config()             const = 0;

    // 供渲染器判断是否需要创建/绑定描述符集与推常量
    virtual bool hasDescriptorSets() const = 0;
    virtual bool hasPushConstants()  const = 0;

    virtual bool isValid() const = 0;
};

// ============================================================================
//  IYalnScene —— 场景容器（场景树 + GPU 缓冲）
//
//  场景 = 一棵以 root 为根的节点树（YalnNode/YalnGroupNode/YalnLeafNode）。
//  update() 每帧自顶向下重算世界矩阵并生成绘制项列表（drawItems），
//  渲染器据此逐项绘制；相同网格的多个实例共享同一份 GPU 缓冲。
// ============================================================================
class YalnNode;

class YALN_CORE_EXPORT IYalnScene
{
public:
    virtual ~IYalnScene() = default;

    // ---- 场景树 ----
    // 替换根节点（接管所有权，整棵旧树被销毁，GPU 缓冲需重新 build）
    virtual void setRoot(std::unique_ptr<YalnNode> root) = 0;
    virtual YalnNode* root() = 0;

    // 每帧调用（渲染器 drawFrame 开头）：重算世界矩阵 + 收集绘制项。
    // cameraPosition 供公告板等行为节点使用。
    virtual void update(const glm::vec3& cameraPosition = glm::vec3(0.0f)) = 0;
    // update() 之后有效；渲染器按此列表逐项绘制
    virtual const std::vector<YalnDrawItem>& drawItems() const = 0;

    // ---- 灯光 ----
    // 场景级光源（Ambient/Directional/Point/Spot），场景拥有所有权；
    // 渲染器每帧打包进 YalnFrameUniform，动态光源上限 YALN_MAX_LIGHTS
    virtual void addLight(std::unique_ptr<YalnLight> light) = 0;
    virtual const std::vector<std::unique_ptr<YalnLight>>& lights() const = 0;

    // ---- 兼容扁平接口：直接挂一个网格到根节点（等价于根节点下一个绘制项）----
    virtual void addMesh(YalnMesh* mesh) = 0;

    // 清空场景树与全部 GPU 缓冲（根节点重置为新的组节点）
    virtual void clear() = 0;

    virtual bool isEmpty() const = 0;   // 整棵树没有任何绘制项
    virtual bool isReady() const = 0;   // GPU 缓冲已构建且有可绘制内容

    // 遍历场景树收集唯一网格并重建全部 GPU 缓冲（调用方需先让设备空闲）
    virtual bool buildGpuBuffers(const IYalnDevice& device, const vk::raii::CommandPool& commandPool) = 0;

    // 去重后的唯一网格列表（buildGpuBuffers 后有效，兼容旧查询）
    virtual const std::vector<YalnMesh*>&   meshes()    const = 0;
    virtual const std::vector<YalnGpuMesh>& gpuMeshes() const = 0;
};

// ============================================================================
//  IYalnRenderer —— 帧循环 GPU 侧状态 + drawFrame（第 7 位）
// ============================================================================
class YALN_CORE_EXPORT IYalnRenderer
{
public:
    virtual ~IYalnRenderer() = default;

    virtual bool initialize(const IYalnDevice&        device,
                            const IYalnSwapChain&     swapChain,
                            const IYalnPipeline&      pipeline,
                            const YalnRendererConfig& config = YalnRendererConfig::makeDefault()) = 0;

    // 交换链重建后调用：重建依赖交换链图像数量的资源
    virtual bool recreate(const IYalnSwapChain& swapChain, const IYalnPipeline& pipeline) = 0;
    virtual void shutdown() = 0;

    // 开头会调用 scene.update() 重算世界矩阵与绘制项
    virtual YalnFrameResult drawFrame(const IYalnSwapChain&  swapChain,
                                      const IYalnPipeline&   pipeline,
                                      const IYalnRenderPass& renderPass,
                                      IYalnScene&            scene,
                                      const YalnCameraPtr&   camera) = 0;

    // 供场景上传网格缓冲时复用
    virtual const vk::raii::CommandPool& commandPool() const = 0;

    virtual const YalnRendererConfig& config() const = 0;
    virtual bool isValid() const = 0;

    // ---- 帧级策略与资源库扩展点 ----
    // 默认实现返回 false/nullptr；自定义渲染器可不支持。
    virtual bool setFramePipeline(std::unique_ptr<IYalnFramePipeline>&& pipeline)
    {
        (void)pipeline;
        return false;
    }

    virtual IYalnFramePipeline* framePipeline() const { return nullptr; }

    virtual YalnPipelineLibrary*       pipelineLibrary()       { return nullptr; }
    virtual const YalnPipelineLibrary* pipelineLibrary() const { return nullptr; }

    virtual YalnMaterialLibrary*       materialLibrary()       { return nullptr; }
    virtual const YalnMaterialLibrary* materialLibrary() const { return nullptr; }
};

#endif // YALN_INTERFACES_H
