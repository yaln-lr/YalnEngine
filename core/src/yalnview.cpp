#include "../include/yalnview.h"
#include "../include/yalnframe.h"

#include <cmath>
#include <fstream>
#include <iostream>
#include <set>

YalnView::YalnView() = default;

YalnView::~YalnView()
{
    // 先让设备空闲并释放帧循环相关资源，再交给成员的逆序析构
    m_renderer->shutdown();
    m_device->waitIdle();
}

// ======================== 实现替换 ========================

#define YALN_VIEW_SET_IMPL(member, name)                                        \
    if (m_initialized)                                                          \
    {                                                                           \
        std::cerr << "[YalnView] " name " 仅可在 initVulkan 之前替换，已忽略" << std::endl; \
        return false;                                                           \
    }                                                                           \
    if (impl == nullptr)                                                        \
    {                                                                           \
        std::cerr << "[YalnView] " name " 不能为空，已忽略" << std::endl;        \
        return false;                                                           \
    }                                                                           \
    member = std::move(impl);                                                   \
    return true;

bool YalnView::setInstance(std::unique_ptr<IYalnInstance> impl)
{
    YALN_VIEW_SET_IMPL(m_instance, "setInstance")
}

bool YalnView::setSurface(std::unique_ptr<IYalnSurface> impl)
{
    YALN_VIEW_SET_IMPL(m_surface, "setSurface")
}

bool YalnView::setDevice(std::unique_ptr<IYalnDevice> impl)
{
    YALN_VIEW_SET_IMPL(m_device, "setDevice")
}

bool YalnView::setSwapChain(std::unique_ptr<IYalnSwapChain> impl)
{
    YALN_VIEW_SET_IMPL(m_swapChain, "setSwapChain")
}

bool YalnView::setRenderPass(std::unique_ptr<IYalnRenderPass> impl)
{
    YALN_VIEW_SET_IMPL(m_renderPass, "setRenderPass")
}

bool YalnView::setShaderProgram(std::unique_ptr<IYalnShaderProgram> impl)
{
    if (m_initialized)
    {
        std::cerr << "[YalnView] setShaderProgram 仅可在 initVulkan 之前替换，已忽略" << std::endl;
        return false;
    }
    m_shaderProgram = std::move(impl);   // 允许显式置回 nullptr（恢复按路径内建）
    return true;
}

bool YalnView::setPipeline(std::unique_ptr<IYalnPipeline> impl)
{
    YALN_VIEW_SET_IMPL(m_pipeline, "setPipeline")
}

bool YalnView::setRenderer(std::unique_ptr<IYalnRenderer> impl)
{
    YALN_VIEW_SET_IMPL(m_renderer, "setRenderer")
}

bool YalnView::setScene(std::unique_ptr<IYalnScene> impl)
{
    YALN_VIEW_SET_IMPL(m_scene, "setScene")
}

bool YalnView::setFramePipeline(std::unique_ptr<IYalnFramePipeline> impl)
{
    if (impl == nullptr)
    {
        std::cerr << "[YalnView] setFramePipeline 不能为空，已忽略" << std::endl;
        return false;
    }

    if (m_initialized)
    {
        if (m_renderer == nullptr)
        {
            return false;
        }
        return m_renderer->setFramePipeline(std::move(impl));
    }

    m_framePipeline = std::move(impl);
    return true;
}

IYalnFramePipeline* YalnView::framePipeline() const
{
    if (m_renderer != nullptr)
    {
        if (IYalnFramePipeline* pipeline = m_renderer->framePipeline())
        {
            return pipeline;
        }
    }
    return m_framePipeline.get();
}

YalnPipelineLibrary* YalnView::pipelineLibrary() const
{
    return m_renderer->pipelineLibrary();
}

YalnMaterialLibrary* YalnView::materialLibrary() const
{
    return m_renderer->materialLibrary();
}

YalnPipelineHandle YalnView::createPipeline(const YalnPipelineConfig& config,
                                            const std::string&        name)
{
    if (!m_initialized || m_renderer == nullptr || m_renderer->pipelineLibrary() == nullptr)
    {
        std::cerr << "[YalnView] createPipeline: 初始化完成后才能创建管线" << std::endl;
        return YALN_INVALID_PIPELINE;
    }
    if (m_shaderProgram == nullptr || !m_shaderProgram->isValid())
    {
        std::cerr << "[YalnView] createPipeline: 当前着色器程序不可用" << std::endl;
        return YALN_INVALID_PIPELINE;
    }

    auto pipeline = std::make_unique<YalnGraphicsPipeline>();
    if (!pipeline->create(*m_device, m_renderPass->handle(), config, *m_shaderProgram))
    {
        std::cerr << "[YalnView] createPipeline: 管线创建失败" << std::endl;
        return YALN_INVALID_PIPELINE;
    }

    return m_renderer->pipelineLibrary()->addPipeline(std::move(pipeline), name);
}

YalnMaterialHandle YalnView::createMaterial(const YalnMaterialDesc& desc)
{
    if (!m_initialized || m_renderer == nullptr || m_renderer->materialLibrary() == nullptr)
    {
        std::cerr << "[YalnView] createMaterial: 初始化完成后才能创建材质" << std::endl;
        return YALN_INVALID_MATERIAL;
    }
    return m_renderer->materialLibrary()->createMaterial(desc);
}

#undef YALN_VIEW_SET_IMPL

// ======================== 初始化编排 ========================
// 不变的流程：实例 → 表面 → 设备 → 交换链 → 组合校验 →
//            渲染通道 → 管线 → 帧缓冲 → 渲染器
// 可变部分两个入口：setXxxConfig（配置扩展）/ setXxx（继承扩展）。
bool YalnView::initVulkan(const char* appName)
{
    if (m_initialized)
    {
        return true;
    }

    try
    {
        // 1. 实例 + 调试信使
        if (!createVulkanInstance(appName))
        {
            m_lastError = "Failed to create Vulkan instance";
            std::cerr << "[YalnView] " << m_lastError << std::endl;
            return false;
        }

        // 2. 窗口表面（由平台层实现并注入）
        if (!createSurface())
        {
            m_lastError = "Failed to create surface";
            std::cerr << "[YalnView] " << m_lastError << std::endl;
            return false;
        }

        // 3. 物理设备 + 逻辑设备 + 队列
        if (!m_device->initialize(*m_instance, *m_surface))
        {
            m_lastError = "Failed to initialize device";
            std::cerr << "[YalnView] " << m_lastError << std::endl;
            return false;
        }

        // 4. 交换链 + 图像视图
        if (!m_swapChain->create(*m_device, *m_surface))
        {
            m_lastError = "Failed to create swap chain";
            std::cerr << "[YalnView] " << m_lastError << std::endl;
            return false;
        }

        // 5. 组合合理性校验（依赖设备能力与交换链信息）
        std::vector<std::string> issues;
        if (!validateConfig(issues))
        {
            for (const std::string& issue : issues)
            {
                std::cerr << "[YalnView] 配置校验失败: " << issue << std::endl;
            }
            m_lastError = issues.front();
            return false;
        }

        // 6. 渲染通道（应用配置）
        if (!m_renderPass->create(*m_device, m_swapChain->format(), m_renderPassConfig))
        {
            m_lastError = "Failed to create render pass";
            std::cerr << "[YalnView] " << m_lastError << std::endl;
            return false;
        }

        // 7. 管线（应用配置 + 着色器程序；未注入程序时按配置路径内建 SPIR-V 实现）
        if (m_shaderProgram == nullptr)
        {
            m_shaderProgram = std::make_unique<YalnSpirvShaderProgram>(
                m_pipelineConfig.vertShaderPath, m_pipelineConfig.fragShaderPath);
        }
        if (!m_shaderProgram->load(*m_device))
        {
            m_lastError = "Failed to load shader program";
            std::cerr << "[YalnView] " << m_lastError << std::endl;
            return false;
        }
        if (!m_pipeline->create(*m_device, m_renderPass->handle(), m_pipelineConfig, *m_shaderProgram))
        {
            m_lastError = "Failed to create graphics pipeline";
            std::cerr << "[YalnView] " << m_lastError << std::endl;
            return false;
        }

        // 8. 帧缓冲（依赖交换链图像视图 + 渲染通道；含可选深度缓冲）
        if (!m_renderPass->createFramebuffers(*m_device, *m_swapChain))
        {
            m_lastError = "Failed to create framebuffers";
            std::cerr << "[YalnView] " << m_lastError << std::endl;
            return false;
        }

        // 9. 帧级策略：在渲染器 initialize 之前注入，避免先创建默认策略再替换
        if (m_framePipeline != nullptr)
        {
            if (!m_renderer->setFramePipeline(std::move(m_framePipeline)))
            {
                m_lastError = "Failed to set frame pipeline";
                std::cerr << "[YalnView] " << m_lastError << std::endl;
                return false;
            }
        }

        // 10. 渲染器：命令池/命令缓冲/同步对象/UBO/描述符（应用配置）
        if (!m_renderer->initialize(*m_device, *m_swapChain, *m_pipeline, m_rendererConfig))
        {
            m_lastError = "Failed to initialize renderer";
            std::cerr << "[YalnView] " << m_lastError << std::endl;
            return false;
        }

        m_initialized = true;
        return true;
    }
    catch (const std::exception& e)
    {
        // 兜底：Vulkan 调用抛出的 vk::SystemError 等一律转为可读错误
        m_lastError = std::string("initVulkan exception: ") + e.what();
        std::cerr << "[YalnView] " << m_lastError << std::endl;
        return false;
    }
}

// ======================== 组合合理性校验 ========================
// 只覆盖"错误组合会导致运行期崩溃/校验层报错"的常见项，
// 并一次性报告所有问题，方便调用方一次改完。
bool YalnView::validateConfig(std::vector<std::string>& issues)
{
    const vk::PhysicalDeviceProperties properties = m_device->physicalDevice().getProperties();
    const vk::PhysicalDeviceLimits&    limits     = properties.limits;
    const vk::PhysicalDeviceFeatures&  features   = m_device->physicalDevice().getFeatures();

    // ---- 1. 着色器文件存在、非空、按 4 字节对齐（SPIR-V 要求）----
    const std::string shaderPaths[2] = { m_pipelineConfig.vertShaderPath, m_pipelineConfig.fragShaderPath };
    for (size_t i = 0; i < 2; i++)
    {
        std::ifstream file(shaderPaths[i], std::ios::ate | std::ios::binary);
        if (!file.is_open())
        {
            issues.push_back("着色器文件不可读: " + shaderPaths[i]);
            continue;
        }
        const std::streamoff size = file.tellg();
        if (size <= 0)
        {
            issues.push_back("着色器文件为空: " + shaderPaths[i]);
        }
        else if (size % 4 != 0)
        {
            issues.push_back("着色器文件大小不是 4 字节倍数（非法 SPIR-V）: " + shaderPaths[i]);
        }
    }

    // ---- 2. 推常量 ----
    const vk::PushConstantRange& pushConstant = m_pipelineConfig.pushConstant;
    if (pushConstant.size > 0)
    {
        if (!bool(pushConstant.stageFlags))
        {
            issues.push_back("管线配置：推常量 size>0 但 stageFlags 为空");
        }
        if (pushConstant.size % 4 != 0)
        {
            issues.push_back("管线配置：推常量大小必须是 4 字节倍数 (当前 " + std::to_string(pushConstant.size) + ")");
        }
        if (pushConstant.size > limits.maxPushConstantsSize)
        {
            issues.push_back("管线配置：推常量大小超过设备上限 " + std::to_string(limits.maxPushConstantsSize)
                             + " (当前 " + std::to_string(pushConstant.size) + ")");
        }
    }

    // ---- 3. 描述符绑定：同一绑定位+类型不得重复 ----
    std::set<std::pair<uint32_t, vk::DescriptorType>> seenBindings;
    bool uniformBindingFound = false;
    for (const vk::DescriptorSetLayoutBinding& binding : m_pipelineConfig.descriptorBindings)
    {
        const auto key = std::make_pair(binding.binding, binding.descriptorType);
        if (!seenBindings.insert(key).second)
        {
            issues.push_back("管线配置：描述符绑定位重复 (binding=" + std::to_string(binding.binding)
                             + ", type=" + vk::to_string(binding.descriptorType) + ")");
        }
        if (binding.binding == m_rendererConfig.uniformBinding
            && binding.descriptorType == vk::DescriptorType::eUniformBuffer)
        {
            uniformBindingFound = true;
        }
    }

    // ---- 4. 渲染器的 UBO 绑定位必须存在于管线描述符绑定中 ----
    if (!m_pipelineConfig.descriptorBindings.empty() && !uniformBindingFound)
    {
        issues.push_back("渲染器配置：uniformBinding=" + std::to_string(m_rendererConfig.uniformBinding)
                         + " 在管线描述符绑定中不存在（或类型不是 eUniformBuffer）");
    }

    // ---- 5. 采样数：渲染通道附件与管线一致，且被设备支持 ----
    if (!(limits.framebufferColorSampleCounts & m_renderPassConfig.samples))
    {
        issues.push_back("渲染通道配置：采样数不被设备 framebufferColorSampleCounts 支持");
    }
    if (m_pipelineConfig.samples != m_renderPassConfig.samples)
    {
        issues.push_back("组合错误：管线采样数必须与渲染通道附件采样数一致 (管线 "
                         + std::to_string(uint32_t(m_pipelineConfig.samples)) + " vs 渲染通道 "
                         + std::to_string(uint32_t(m_renderPassConfig.samples)) + ")");
    }

    // ---- 6. 深度：渲染通道与管线必须一致，且设备支持深度格式 ----
    if (m_renderPassConfig.enableDepth)
    {
        bool depthFormatSupported = false;
        const vk::Format depthCandidates[3] = {
            vk::Format::eD32Sfloat, vk::Format::eD24UnormS8Uint, vk::Format::eD32SfloatS8Uint
        };
        for (const vk::Format format : depthCandidates)
        {
            const vk::FormatProperties props = m_device->physicalDevice().getFormatProperties(format);
            if (props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment)
            {
                depthFormatSupported = true;
                break;
            }
        }
        if (!depthFormatSupported)
        {
            issues.push_back("渲染通道配置：enableDepth=true 但设备不支持任何常用深度格式");
        }
    }
    if ((m_pipelineConfig.depthTest || m_pipelineConfig.depthWrite) && !m_renderPassConfig.enableDepth)
    {
        issues.push_back("组合错误：管线启用深度测试/写入，但渲染通道未启用深度附件");
    }

    // ---- 7. 同时飞行帧数 ∈ [1, 交换链图像数量] ----
    const uint32_t imageCount = static_cast<uint32_t>(m_swapChain->images().size());
    if (m_rendererConfig.maxFramesInFlight < 1)
    {
        issues.push_back("渲染器配置：maxFramesInFlight 必须 ≥ 1");
    }
    if (m_rendererConfig.maxFramesInFlight > imageCount)
    {
        issues.push_back("渲染器配置：maxFramesInFlight (" + std::to_string(m_rendererConfig.maxFramesInFlight)
                         + ") 超过交换链图像数量 (" + std::to_string(imageCount) + ")");
    }

    // ---- 8. 设备特性 ----
    if (m_pipelineConfig.polygonMode != vk::PolygonMode::eFill && !features.fillModeNonSolid)
    {
        issues.push_back("管线配置：polygonMode 非 Fill 需要 fillModeNonSolid 特性，当前设备不支持");
    }
    if (m_pipelineConfig.lineWidth > 1.0f && !features.wideLines)
    {
        issues.push_back("管线配置：lineWidth>1 需要 wideLines 特性，当前设备不支持");
    }

    // ---- 9. 清除色：非有限值视为配置错误 ----
    for (const float channel : m_rendererConfig.clearColor)
    {
        if (!std::isfinite(channel))
        {
            issues.push_back("渲染器配置：清除色包含非有限值");
            break;
        }
    }

    return issues.empty();
}

bool YalnView::createVulkanInstance(const char* appName)
{
    return m_instance->create(appName, getRequiredInstanceExtensions());
}

std::vector<const char*> YalnView::getRequiredInstanceExtensions()
{
    std::vector<const char*> requiredExtensions = {
        VK_KHR_SURFACE_EXTENSION_NAME
    };

#ifdef _WIN32
    requiredExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(__linux__)
    requiredExtensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#elif defined(__APPLE__)
    requiredExtensions.push_back(VK_MVK_MACOS_SURFACE_EXTENSION_NAME);
#endif

    if (YalnVulkanInstance::validationLayerEnabled())
    {
        requiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return requiredExtensions;
}

// ======================== 交换链重建 ========================

void YalnView::recreateSwapChain()
{
    if (!m_initialized)
    {
        return;
    }

    try
    {
        m_device->waitIdle();

        m_renderPass->destroyFramebuffers();
        m_swapChain->destroy();

        if (!m_swapChain->create(*m_device, *m_surface))
        {
            std::cerr << "[YalnView] Failed to recreate swap chain" << std::endl;
            return;
        }

        if (!m_renderPass->createFramebuffers(*m_device, *m_swapChain))
        {
            std::cerr << "[YalnView] Failed to recreate framebuffers" << std::endl;
            return;
        }

        // 图像数量可能变化，命令缓冲/描述符/同步对象需要一起重建
        m_renderer->recreate(*m_swapChain, *m_pipeline);
    }
    catch (const std::exception& e)
    {
        std::cerr << "[YalnView] recreateSwapChain exception: " << e.what() << std::endl;
    }
}

// ======================== 帧循环 ========================

void YalnView::drawFrame()
{
    if (!m_initialized)
    {
        return;
    }

    try
    {
        const auto result = m_renderer->drawFrame(*m_swapChain, *m_pipeline, *m_renderPass, *m_scene, m_camera_ptr);
        if (result == YalnFrameResult::SwapChainOutOfDate)
        {
            recreateSwapChain();
        }
    }
    catch (const std::exception& e)
    {
        // 帧循环异常不能让平台层崩溃：记录后跳过本帧
        std::cerr << "[YalnView] drawFrame exception: " << e.what() << std::endl;
    }
}

// ======================== 场景 ========================

void YalnView::addMesh(YalnMesh* mesh)
{
    m_scene->addMesh(mesh);

    if (m_initialized && mesh != nullptr && mesh->isValid())
    {
        createMeshBuffers();
    }
}

void YalnView::createMeshBuffers()
{
    m_device->waitIdle();
    m_scene->buildGpuBuffers(*m_device, m_renderer->commandPool());
}

void YalnView::clearMeshes()
{
    m_device->waitIdle();
    m_scene->clear();
}

bool YalnView::isMeshReady() const
{
    return m_scene->isReady();
}

bool YalnView::rebuildScene()
{
    if (!m_initialized)
    {
        std::cerr << "[YalnView] rebuildScene: Vulkan 尚未初始化" << std::endl;
        return false;
    }
    m_device->waitIdle();
    return m_scene->buildGpuBuffers(*m_device, m_renderer->commandPool());
}

// ======================== 其它 ========================

void YalnView::setCameta(YalnCameraPtr camera)
{
    m_camera_ptr = std::move(camera);
}

void YalnView::printCapacity()
{
    // printCapacity 是默认实现的诊断方法，不在接口契约内
    if (auto* device = dynamic_cast<const YalnDevice*>(m_device.get()))
    {
        device->printCapacity();
    }
    else
    {
        std::cerr << "[YalnView] 当前设备实现不支持 printCapacity" << std::endl;
    }
}
