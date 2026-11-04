#ifndef YALN_RENDERER_H
#define YALN_RENDERER_H

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <vector>

#include "global_type.h"
#include "yalnbuffer.h"
#include "yalncamera.h"

class YalnDevice;
class YalnSwapChain;
class YalnRenderPass;
class YalnGraphicsPipeline;
class YalnScene;

// 同时在飞的帧数
constexpr uint32_t YALN_MAX_FRAMES_IN_FLIGHT = 2;

// 每帧的 uniform 数据（仅投影-视图矩阵）
struct YALN_CORE_EXPORT YalnFrameUniform {
    alignas(16) glm::mat4 proj_view_mat;
};

// ============================================================================
//  YalnRenderer
//  职责：帧循环相关的全部 GPU 侧状态——
//        命令池/命令缓冲、同步对象（信号量/栅栏）、每图像 uniform 缓冲、
//        描述符池/描述符集，以及 drawFrame 的命令录制与提交。
//        不持有交换链、管线、场景的创建逻辑，只使用它们。
// ============================================================================
class YALN_CORE_EXPORT YalnRenderer
{
public:
    enum class FrameResult {
        Rendered,            // 正常出帧
        SwapChainOutOfDate,  // 需要重建交换链
        NotReady,            // 场景尚未就绪，跳过
        Failed               // 提交/获取失败
    };

    YalnRenderer() = default;

    YalnRenderer(const YalnRenderer&)            = delete;
    YalnRenderer& operator=(const YalnRenderer&) = delete;

    // 创建命令池、uniform 缓冲、描述符、命令缓冲、同步对象
    bool initialize(const YalnDevice&         device,
                    const YalnSwapChain&      swapChain,
                    const YalnGraphicsPipeline& pipeline);

    // 交换链重建后调用：重建依赖交换链图像数量的资源
    bool recreate(const YalnSwapChain& swapChain, const YalnGraphicsPipeline& pipeline);

    void shutdown();

    FrameResult drawFrame(const YalnSwapChain&        swapChain,
                          const YalnGraphicsPipeline& pipeline,
                          const YalnRenderPass&       renderPass,
                          const YalnScene&            scene,
                          const YalnCameraPtr&        camera);

    // 供场景上传网格缓冲时复用
    const vk::raii::CommandPool& commandPool() const { return m_commandPool; }

    bool isValid() const { return m_initialized; }

private:
    bool createCommandPool();
    bool createUniformBuffers(const YalnSwapChain& swapChain);
    bool createDescriptorPool(const YalnSwapChain& swapChain);
    bool createDescriptorSets(const YalnSwapChain& swapChain, const YalnGraphicsPipeline& pipeline);
    bool createCommandBuffers(const YalnSwapChain& swapChain);
    bool createSyncObjects(const YalnSwapChain& swapChain);
    void destroyFrameResources();

    void recordCommandBuffer(vk::CommandBuffer                  commandBuffer,
                             uint32_t                           imageIndex,
                             const YalnSwapChain&               swapChain,
                             const YalnGraphicsPipeline&        pipeline,
                             const YalnRenderPass&              renderPass,
                             const YalnScene&                   scene);

    const YalnDevice* m_device = nullptr;
    bool              m_initialized = false;

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
