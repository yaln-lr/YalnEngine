#include "../include/yalnrenderer.h"
#include "../include/yalndevice.h"
#include "../include/yalnswapchain.h"
#include "../include/yalnrenderpass.h"
#include "../include/yalngraphicspipeline.h"
#include "../include/yalnscene.h"
#include "../include/yalnmesh.h"
#include "../include/yalnframe.h"
#include "../include/yalnforwardpipeline.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <utility>

YalnRenderer::~YalnRenderer() = default;

// ======================== 初始化 ========================

bool YalnRenderer::initialize(const IYalnDevice&        device,
                              const IYalnSwapChain&     swapChain,
                              const IYalnPipeline&      pipeline,
                              const YalnRendererConfig& config)
{
    m_device    = &device;
    m_swapChain = &swapChain;
    m_config    = config;

    // 同时飞行帧数不能超过交换链图像数量（防御性钳制，组合校验在 YalnView）
    m_maxFramesInFlight = std::clamp(config.maxFramesInFlight, 1u,
                                     static_cast<uint32_t>(swapChain.images().size()));

    if (!createCommandPool())                          return false;
    if (!createUniformBuffers(swapChain))              return false;
    if (!createDescriptorPool(swapChain))              return false;
    if (!createDescriptorSets(swapChain, pipeline))    return false;
    if (!createCommandBuffers(swapChain))              return false;
    if (!createSyncObjects(swapChain))                 return false;

    // 默认 PSO 句柄 0：登记 YalnView 持有的默认管线（不接管所有权）
    m_pipelineLibrary.clear();
    const YalnPipelineHandle defaultPipeline = m_pipelineLibrary.addExternal(pipeline, "Default");

    // 默认材质句柄 0：白色乘色 + 默认管线 + 参与全部 Pass
    m_materialLibrary.clear();
    m_materialLibrary.ensureDefault(defaultPipeline);

    // 帧级策略：若调用方未预先注入，则使用默认 Forward
    if (m_framePipeline == nullptr)
    {
        m_framePipeline = std::make_unique<YalnForwardFramePipeline>();
    }
    if (!m_framePipeline->initialize(device, swapChain))
    {
        return false;
    }

    m_currentFrame = 0;
    m_initialized  = true;
    return true;
}

bool YalnRenderer::recreate(const IYalnSwapChain& swapChain, const IYalnPipeline& pipeline)
{
    if (!m_initialized)
    {
        return false;
    }

    m_swapChain = &swapChain;

    m_device->waitIdle();
    destroyFrameResources();

    if (!createUniformBuffers(swapChain))           return false;
    if (!createDescriptorPool(swapChain))           return false;
    if (!createDescriptorSets(swapChain, pipeline)) return false;
    if (!createCommandBuffers(swapChain))           return false;
    if (!createSyncObjects(swapChain))              return false;

    // 默认管线可能已被替换：库为空时补登记
    if (m_pipelineLibrary.size() == 0)
    {
        const YalnPipelineHandle defaultPipeline = m_pipelineLibrary.addExternal(pipeline, "Default");
        m_materialLibrary.ensureDefault(defaultPipeline);
    }

    if (m_framePipeline != nullptr && !m_framePipeline->recreate(swapChain))
    {
        return false;
    }

    m_currentFrame = 0;
    return true;
}

void YalnRenderer::shutdown()
{
    if (m_device != nullptr)
    {
        m_device->waitIdle();
    }

    if (m_framePipeline != nullptr)
    {
        m_framePipeline->shutdown();
        m_framePipeline.reset();
    }

    destroyFrameResources();
    m_pipelineLibrary.clear();
    m_materialLibrary.clear();
    m_commandPool = nullptr;
    m_swapChain = nullptr;
    m_initialized = false;
}

bool YalnRenderer::setFramePipeline(std::unique_ptr<IYalnFramePipeline>&& pipeline)
{
    if (pipeline == nullptr)
    {
        return false;
    }

    // 初始化后热替换：先确认新策略可初始化，再切换并关闭旧策略
    if (m_initialized && m_device != nullptr && m_swapChain != nullptr)
    {
        m_device->waitIdle();
        if (!pipeline->initialize(*m_device, *m_swapChain))
        {
            return false;
        }
        if (m_framePipeline != nullptr)
        {
            m_framePipeline->shutdown();
        }
        m_framePipeline = std::move(pipeline);
        return true;
    }

    // 初始化前注入：保存，initialize() 时统一初始化
    if (m_framePipeline != nullptr)
    {
        m_framePipeline->shutdown();
    }
    m_framePipeline = std::move(pipeline);
    return true;
}

IYalnFramePipeline* YalnRenderer::framePipeline() const
{
    return m_framePipeline.get();
}

bool YalnRenderer::createCommandPool()
{
    const vk::CommandPoolCreateInfo poolInfo(
        vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        m_device->queueFamilies().graphicsFamily.value());

    m_commandPool = vk::raii::CommandPool(m_device->device(), poolInfo);
    return m_commandPool != nullptr;
}

bool YalnRenderer::createUniformBuffers(const IYalnSwapChain& swapChain)
{
    const vk::DeviceSize bufferSize = sizeof(YalnFrameUniform);

    m_uniformBuffers.clear();
    m_uniformBuffers.resize(swapChain.images().size());

    for (auto& uniformBuffer : m_uniformBuffers)
    {
        if (!uniformBuffer.create(*m_device, bufferSize,
                                  vk::BufferUsageFlagBits::eUniformBuffer,
                                  vk::MemoryPropertyFlagBits::eHostVisible |
                                  vk::MemoryPropertyFlagBits::eHostCoherent))
        {
            return false;
        }
    }

    return true;
}

bool YalnRenderer::createDescriptorPool(const IYalnSwapChain& swapChain)
{
    const uint32_t imageCount = static_cast<uint32_t>(swapChain.images().size());

    const std::array<vk::DescriptorPoolSize, 1> poolSizes = {{
        { vk::DescriptorType::eUniformBuffer, imageCount }
    }};

    const vk::DescriptorPoolCreateInfo poolInfo(
        vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        imageCount,
        static_cast<uint32_t>(poolSizes.size()),
        poolSizes.data());

    m_descriptorPool = vk::raii::DescriptorPool(m_device->device(), poolInfo);
    return m_descriptorPool != nullptr;
}

bool YalnRenderer::createDescriptorSets(const IYalnSwapChain& swapChain, const IYalnPipeline& pipeline)
{
    m_descriptorSets.clear();

    // 管线未定义描述符布局：跳过描述符创建（录制时也不再绑定）
    if (!pipeline.hasDescriptorSets())
    {
        return true;
    }

    const size_t imageCount = swapChain.images().size();
    if (m_uniformBuffers.size() != imageCount)
    {
        return false;
    }

    const std::vector<vk::DescriptorSetLayout> layouts(imageCount, *pipeline.descriptorSetLayout());

    const vk::DescriptorSetAllocateInfo allocInfo(
        *m_descriptorPool,
        static_cast<uint32_t>(layouts.size()),
        layouts.data());

    m_descriptorSets = m_device->device().allocateDescriptorSets(allocInfo);

    for (size_t i = 0; i < m_descriptorSets.size(); i++)
    {
        const vk::DescriptorBufferInfo bufferInfo(
            m_uniformBuffers[i].handle(),
            0,
            sizeof(YalnFrameUniform));

        const std::array<vk::WriteDescriptorSet, 1> descriptorWrites = {
            vk::WriteDescriptorSet(
                *m_descriptorSets[i],
                m_config.uniformBinding, 0, 1,
                vk::DescriptorType::eUniformBuffer,
                nullptr,
                &bufferInfo,
                nullptr)
        };

        m_device->device().updateDescriptorSets(descriptorWrites, {});
    }

    return true;
}

bool YalnRenderer::createCommandBuffers(const IYalnSwapChain& swapChain)
{
    m_commandBuffers.clear();

    const vk::CommandBufferAllocateInfo allocInfo(
        *m_commandPool,
        vk::CommandBufferLevel::ePrimary,
        static_cast<uint32_t>(swapChain.images().size()));

    m_commandBuffers = m_device->device().allocateCommandBuffers(allocInfo);
    return !m_commandBuffers.empty();
}

bool YalnRenderer::createSyncObjects(const IYalnSwapChain& swapChain)
{
    const size_t imageCount = swapChain.images().size();

    m_imageAvailableSemaphores.clear();
    m_renderFinishedSemaphores.clear();
    m_inFlightFences.clear();
    m_imagesInFlight.assign(imageCount, VK_NULL_HANDLE);

    const vk::SemaphoreCreateInfo semaphoreInfo{};
    const vk::FenceCreateInfo     fenceInfo(vk::FenceCreateFlagBits::eSignaled);

    // 每个交换链图像一对信号量，避免复用冲突
    for (size_t i = 0; i < imageCount; i++)
    {
        m_imageAvailableSemaphores.emplace_back(m_device->device(), semaphoreInfo);
        m_renderFinishedSemaphores.emplace_back(m_device->device(), semaphoreInfo);
    }

    // 栅栏只需要同时飞行的帧数
    for (size_t i = 0; i < m_maxFramesInFlight; i++)
    {
        m_inFlightFences.emplace_back(m_device->device(), fenceInfo);
    }

    return true;
}

void YalnRenderer::destroyFrameResources()
{
    m_descriptorSets.clear();
    m_commandBuffers.clear();
    m_uniformBuffers.clear();

    m_descriptorPool = nullptr;

    m_imageAvailableSemaphores.clear();
    m_renderFinishedSemaphores.clear();
    m_inFlightFences.clear();
    m_imagesInFlight.clear();
}

// ======================== 帧循环 ========================

YalnFrameResult YalnRenderer::drawFrame(const IYalnSwapChain&  swapChain,
                                       const IYalnPipeline&   pipeline,
                            const IYalnRenderPass& renderPass,
                            IYalnScene&            scene,
                                                  const YalnCameraPtr&        camera)
{
    if (!m_initialized || camera == nullptr)
    {
        return FrameResult::NotReady;
    }

    (void)pipeline; // 管线的选择已下沉到材质/管线库；保留参数用于兼容旧调用

    if (!scene.isReady() || renderPass.framebuffers().empty())
    {
        return FrameResult::NotReady;
    }

    // 每帧重算场景树世界矩阵并收集可见绘制项（相机位置供公告板使用）
    scene.update(camera->getPosition());

    const auto& device = m_device->device();

    // 等待本帧栅栏
    auto& fence = m_inFlightFences[m_currentFrame];
    const auto waitResult = device.waitForFences({ *fence }, VK_TRUE, std::numeric_limits<uint64_t>::max());
    if (waitResult != vk::Result::eSuccess)
    {
        std::cerr << "waitForFences failed" << std::endl;
        return FrameResult::Failed;
    }

    // 获取交换链图像
    auto [acquireResult, imageIndex] = swapChain.handle().acquireNextImage(
        std::numeric_limits<uint64_t>::max(),
        *m_imageAvailableSemaphores[m_currentFrame],
        VK_NULL_HANDLE);

    if (acquireResult == vk::Result::eErrorOutOfDateKHR || acquireResult == vk::Result::eSuboptimalKHR)
    {
        return FrameResult::SwapChainOutOfDate;
    }

    // 更新 uniform：投影视图矩阵 + 相机位置 + 灯光打包
    YalnFrameUniform ubo{};
    ubo.proj_view_mat = camera->getMatrix();
    ubo.camera_pos    = glm::vec4(camera->getPosition(), 0.0f);

    // 环境光兜底：场景没加环境光时也给一点底色，避免全黑
    ubo.ambient = glm::vec4(0.25f, 0.25f, 0.28f, 0.0f);

    uint32_t lightCount = 0;
    for (const auto& light : scene.lights())
    {
        if (light->type() == YalnLightType::Ambient)
        {
            // 环境光独立通道：多盏时后添加的生效
            ubo.ambient = glm::vec4(light->color() * light->intensity(), 0.0f);
            continue;
        }
        if (lightCount >= YALN_MAX_LIGHTS)
        {
            std::cerr << "YalnRenderer: 动态光源超过上限 " << YALN_MAX_LIGHTS
                      << "，多余的已被忽略" << std::endl;
            break;
        }
        ubo.lights[lightCount++] = light->gpuData();
    }
    ubo.counts = glm::ivec4(static_cast<int>(lightCount), 0, 0, 0);

    m_uniformBuffers[imageIndex].write(&ubo, sizeof(ubo));

    // 录制
    recordCommandBuffer(*m_commandBuffers[imageIndex], imageIndex,
                        swapChain, renderPass, scene, *camera);

    // 该图像是否仍被旧帧使用
    if (m_imagesInFlight[imageIndex] != VK_NULL_HANDLE)
    {
        const std::array<vk::Fence, 1> fences = { m_imagesInFlight[imageIndex] };
        device.waitForFences(fences, VK_TRUE, std::numeric_limits<uint64_t>::max());
    }
    m_imagesInFlight[imageIndex] = *fence;

    device.resetFences({ *fence });

    const std::array<vk::PipelineStageFlags, 1> waitStages = {
        vk::PipelineStageFlagBits::eColorAttachmentOutput
    };

    const vk::SubmitInfo submitInfo(
        1, &*m_imageAvailableSemaphores[m_currentFrame],
        waitStages.data(),
        1, &*m_commandBuffers[imageIndex],
        1, &*m_renderFinishedSemaphores[imageIndex]);

    m_device->graphicsQueue().submit(submitInfo, *fence);

    const std::array<vk::SwapchainKHR, 1> swapChains = { *swapChain.handle() };
    const vk::PresentInfoKHR presentInfo(
        1, &*m_renderFinishedSemaphores[imageIndex],
        static_cast<uint32_t>(swapChains.size()),
        swapChains.data(),
        &imageIndex);

    const auto presentResult = m_device->presentQueue().presentKHR(presentInfo);

    m_currentFrame = (m_currentFrame + 1) % m_maxFramesInFlight;

    if (presentResult == vk::Result::eErrorOutOfDateKHR || presentResult == vk::Result::eSuboptimalKHR)
    {
        return FrameResult::SwapChainOutOfDate;
    }

    return FrameResult::Rendered;
}

void YalnRenderer::recordCommandBuffer(vk::CommandBuffer          commandBuffer,
                                       uint32_t                   imageIndex,
                                       const IYalnSwapChain&      swapChain,
                                       const IYalnRenderPass&     renderPass,
                                       const IYalnScene&          scene,
                                       const YalnCamera&          camera)
{
    commandBuffer.reset();

    const vk::CommandBufferBeginInfo beginInfo{};
    commandBuffer.begin(beginInfo);

    if (m_framePipeline != nullptr)
    {
        YalnFrameContext context;
        context.commandBuffer  = commandBuffer;
        context.imageIndex     = imageIndex;
        context.swapChain      = &swapChain;
        context.renderPass     = &renderPass;
        context.scene          = &scene;
        context.camera         = &camera;
        context.rendererConfig = &m_config;
        context.pipelines      = &m_pipelineLibrary;
        context.materials      = &m_materialLibrary;

        if (!m_descriptorSets.empty() && imageIndex < m_descriptorSets.size())
        {
            context.frameDescriptorSet = *m_descriptorSets[imageIndex];
        }

        m_framePipeline->record(context);
    }

    commandBuffer.end();
}
