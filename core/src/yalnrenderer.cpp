#include "../include/yalnrenderer.h"
#include "../include/yalndevice.h"
#include "../include/yalnswapchain.h"
#include "../include/yalnrenderpass.h"
#include "../include/yalngraphicspipeline.h"
#include "../include/yalnscene.h"
#include "../include/yalnmesh.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <limits>

// ======================== 初始化 ========================

bool YalnRenderer::initialize(const IYalnDevice&        device,
                              const IYalnSwapChain&     swapChain,
                              const IYalnPipeline&      pipeline,
                              const YalnRendererConfig& config)
{
    m_device = &device;
    m_config = config;

    // 同时飞行帧数不能超过交换链图像数量（防御性钳制，组合校验在 YalnView）
    m_maxFramesInFlight = std::clamp(config.maxFramesInFlight, 1u,
                                     static_cast<uint32_t>(swapChain.images().size()));

    if (!createCommandPool())                          return false;
    if (!createUniformBuffers(swapChain))              return false;
    if (!createDescriptorPool(swapChain))              return false;
    if (!createDescriptorSets(swapChain, pipeline))    return false;
    if (!createCommandBuffers(swapChain))              return false;
    if (!createSyncObjects(swapChain))                 return false;

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

    m_device->waitIdle();
    destroyFrameResources();

    if (!createUniformBuffers(swapChain))           return false;
    if (!createDescriptorPool(swapChain))           return false;
    if (!createDescriptorSets(swapChain, pipeline)) return false;
    if (!createCommandBuffers(swapChain))           return false;
    if (!createSyncObjects(swapChain))              return false;

    m_currentFrame = 0;
    return true;
}

void YalnRenderer::shutdown()
{
    if (m_device != nullptr)
    {
        m_device->waitIdle();
    }

    destroyFrameResources();
    m_commandPool = nullptr;
    m_initialized = false;
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

    // 更新 uniform（仅投影-视图矩阵）
    YalnFrameUniform ubo{};
    ubo.proj_view_mat = camera->getMatrix();
    m_uniformBuffers[imageIndex].write(&ubo, sizeof(ubo));

    // 录制
    recordCommandBuffer(*m_commandBuffers[imageIndex], imageIndex,
                        swapChain, pipeline, renderPass, scene);

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
                             const IYalnPipeline&       pipeline,
                             const IYalnRenderPass&     renderPass,
                             const IYalnScene&          scene)
{
    const vk::Extent2D extent = swapChain.extent();

    commandBuffer.reset();

    const vk::CommandBufferBeginInfo beginInfo{};
    commandBuffer.begin(beginInfo);

    // 清除值必须覆盖渲染通道全部 loadOp=eClear 的附件（VUID-00902）：
    // 深度附件存在时需附带 DepthStencil 清除值（深度清 1 = 最远）
    const vk::ClearColorValue clearColor(m_config.clearColor);
    std::array<vk::ClearValue, 2> clearValues = {
        vk::ClearValue(clearColor),
        vk::ClearValue(vk::ClearDepthStencilValue(1.0f, 0))
    };
    const uint32_t clearCount = renderPass.hasDepth() ? 2u : 1u;

    const vk::RenderPassBeginInfo renderPassInfo(
        *renderPass.handle(),
        *renderPass.framebuffers()[imageIndex],
        { { 0, 0 }, extent },
        clearCount, clearValues.data());

    commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline.handle());

    const vk::Viewport viewport(0.0f, 0.0f,
                                static_cast<float>(extent.width),
                                static_cast<float>(extent.height),
                                0.0f, 1.0f);
    commandBuffer.setViewport(0, viewport);
    commandBuffer.setScissor(0, vk::Rect2D({ 0, 0 }, extent));

    // 描述符集（管线未声明描述符布局时跳过）
    if (!m_descriptorSets.empty())
    {
        commandBuffer.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            *pipeline.layout(),
            0, *m_descriptorSets[imageIndex],
            {});
    }

    // 场景树收集的绘制项（同一网格多处实例共享一份 GPU 缓冲）
    for (const YalnDrawItem& item : scene.drawItems())
    {
        const YalnGpuMesh& gpuMesh = *item.gpuMesh;

        commandBuffer.bindVertexBuffers(0, { gpuMesh.vertexBuffer.handle() }, { 0 });
        commandBuffer.bindIndexBuffer(gpuMesh.indexBuffer.handle(), 0, vk::IndexType::eUint32);

        // Push Constants：世界矩阵（管线未声明推常量时跳过）
        if (pipeline.hasPushConstants())
        {
            constexpr uint32_t size = sizeof(glm::mat4);
            commandBuffer.pushConstants(
                *pipeline.layout(),
                vk::ShaderStageFlags{ vk::ShaderStageFlagBits::eVertex },
                0u,
                vk::ArrayProxy<const float>(size / sizeof(float), &item.world[0][0]));
        }

        commandBuffer.drawIndexed(gpuMesh.indexCount, 1, 0, 0, 0);
    }

    commandBuffer.endRenderPass();
    commandBuffer.end();
}
