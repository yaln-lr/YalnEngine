#ifndef YALN_RENDERER_H
#define YALN_RENDERER_H

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <vector>

#include "global_type.h"
#include "yalninterfaces.h"

// 同时在飞行的帧数（默认值，可通过 YalnRendererConfig 覆盖）
constexpr uint32_t YALN_MAX_FRAMES_IN_FLIGHT = 2;

// 动态光源（平行/点/聚光）数量上限；环境光走独立通道不受此限
inline constexpr uint32_t YALN_MAX_LIGHTS = 8;

// 每帧的 uniform 数据（std140，与 shader.vert/frag 的 UniformBufferObject 一一对应）
struct YALN_CORE_EXPORT YalnFrameUniform {
    alignas(16) glm::mat4 proj_view_mat;
    alignas(16) glm::vec4 camera_pos;    // xyz = 相机世界坐标（高光/衰减计算用）
    alignas(16) glm::vec4 ambient;       // rgb = 环境光色 × 强度（无环境光时渲染器兜底）
    alignas(16) glm::ivec4 counts;       // x = 动态光源数量
    alignas(16) YalnGPULight lights[YALN_MAX_LIGHTS];
};

// ============================================================================
//  YalnRenderer —— IYalnRenderer 默认实现
//  职责：帧循环相关的全部 GPU 侧状态——
//        命令池/命令缓冲、同步对象（信号量/栅栏）、每图像 uniform 缓冲、
//        描述符池/描述符集，以及 drawFrame 的命令录制与提交。
// ============================================================================
class YALN_CORE_EXPORT YalnRenderer : public IYalnRenderer
{
public:
    // 兼容别名：枚举定义已提升为接口层共享类型
    using FrameResult = YalnFrameResult;

    YalnRenderer() = default;

    YalnRenderer(const YalnRenderer&)            = delete;
    YalnRenderer& operator=(const YalnRenderer&) = delete;

    bool initialize(const IYalnDevice&        device,
                    const IYalnSwapChain&     swapChain,
                    const IYalnPipeline&      pipeline,
                    const YalnRendererConfig& config = YalnRendererConfig::makeDefault()) override;

    // 交换链重建后调用：重建依赖交换链图像数量的资源
    bool recreate(const IYalnSwapChain& swapChain, const IYalnPipeline& pipeline) override;

    void shutdown() override;

    YalnFrameResult drawFrame(const IYalnSwapChain&  swapChain,
                              const IYalnPipeline&   pipeline,
                              const IYalnRenderPass& renderPass,
                              IYalnScene&            scene,
                              const YalnCameraPtr&   camera) override;

    // 供场景上传网格缓冲时复用
    const vk::raii::CommandPool& commandPool() const override { return m_commandPool; }

    const YalnRendererConfig& config() const override { return m_config; }
    bool isValid() const override { return m_initialized; }

private:
    bool createCommandPool();
    bool createUniformBuffers(const IYalnSwapChain& swapChain);
    bool createDescriptorPool(const IYalnSwapChain& swapChain);
    bool createDescriptorSets(const IYalnSwapChain& swapChain, const IYalnPipeline& pipeline);
    bool createCommandBuffers(const IYalnSwapChain& swapChain);
    bool createSyncObjects(const IYalnSwapChain& swapChain);
    void destroyFrameResources();

    void recordCommandBuffer(vk::CommandBuffer          commandBuffer,
                             uint32_t                   imageIndex,
                             const IYalnSwapChain&      swapChain,
                             const IYalnPipeline&       pipeline,
                             const IYalnRenderPass&     renderPass,
                             const IYalnScene&          scene);

    const IYalnDevice* m_device = nullptr;
    bool               m_initialized = false;

    YalnRendererConfig m_config;
    uint32_t           m_maxFramesInFlight = YALN_MAX_FRAMES_IN_FLIGHT;

    vk::raii::CommandPool m_commandPool{ nullptr };

    std::vector<vk::raii::CommandBuffer> m_commandBuffers;
    std::vector<YalnBuffer>              m_uniformBuffers;

    vk::raii::DescriptorPool                 m_descriptorPool{ nullptr };
    std::vector<vk::raii::DescriptorSet>     m_descriptorSets;

    std::vector<vk::raii::Semaphore> m_imageAvailableSemaphores;
    std::vector<vk::raii::Semaphore> m_renderFinishedSemaphores;
    std::vector<vk::raii::Fence>     m_inFlightFences;
    std::vector<vk::Fence>           m_imagesInFlight;

    uint32_t m_currentFrame = 0;
};

#endif // YALN_RENDERER_H
