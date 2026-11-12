#include "../include/yalngraphicspipeline.h"
#include "../include/yalnshaderprogram.h"
#include "../include/yalninterfaces.h"
#include "../include/yalnvertex.h"

#include <array>
#include <iostream>

// 默认配置：与既有 shader.vert/frag（UBO 投影视图矩阵 + 推常量模型矩阵）匹配
YalnPipelineConfig YalnPipelineConfig::makeDefault()
{
    YalnPipelineConfig config;
    config.vertShaderPath = "../shaders/shader.vert.spv";
    config.fragShaderPath = "../shaders/shader.frag.spv";
    config.descriptorBindings.push_back(vk::DescriptorSetLayoutBinding(
        0,
        vk::DescriptorType::eUniformBuffer,
        1,
        vk::ShaderStageFlagBits::eVertex,
        nullptr));
    return config;
}

bool YalnGraphicsPipeline::create(const IYalnDevice&          device,
                                  const vk::raii::RenderPass& renderPass,
                                  const YalnPipelineConfig&   config,
                                  const IYalnShaderProgram&   shaderProgram)
{
    destroy();
    m_config = config;

    if (!shaderProgram.isValid())
    {
        std::cerr << "[YalnGraphicsPipeline] 着色器程序尚未加载（请先调用 load）" << std::endl;
        return false;
    }

    // 图形管线要求图形程序（Vertex + Fragment 必须存在，规则详见 IYalnShaderProgram）
    if (shaderProgram.type() != IYalnShaderProgram::Type::Graphics)
    {
        std::cerr << "[YalnGraphicsPipeline] 着色器程序类型不是 Graphics，无法用于图形管线"
                  << "（Compute 程序请使用计算管线）" << std::endl;
        return false;
    }

    // ---- 描述符集布局（可空）----
    if (!m_config.descriptorBindings.empty())
    {
        vk::DescriptorSetLayoutCreateInfo layoutInfo({}, m_config.descriptorBindings);
        m_descriptorSetLayout = vk::raii::DescriptorSetLayout(device.device(), layoutInfo);
    }

    // ---- 着色器阶段（由着色器程序提供，与加载来源解耦）----
    const std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = shaderProgram.stageInfos();

    // 顶点输入（当前固定为 YalnVertexPC：位置 + 颜色）
    const auto bindingDescription    = YalnVertexPC::getBindingDescription();
    const auto attributeDescriptions = YalnVertexPC::getAttributeDescriptions();

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo(
        {},
        1, reinterpret_cast<const vk::VertexInputBindingDescription*>(&bindingDescription),
        static_cast<uint32_t>(attributeDescriptions.size()),
        reinterpret_cast<const vk::VertexInputAttributeDescription*>(attributeDescriptions.data()));

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly(
        {},
        m_config.topology,
        VK_FALSE);

    // 动态视口 + 裁剪：窗口缩放时无需重建管线
    vk::PipelineDynamicStateCreateInfo dynamicStateInfo{};
    const std::array<vk::DynamicState, 2> dynamicStates = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor
    };
    dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicStateInfo.pDynamicStates    = dynamicStates.data();

    vk::PipelineViewportStateCreateInfo viewportState({}, 1, nullptr, 1, nullptr);

    vk::PipelineRasterizationStateCreateInfo rasterizer(
        {},
        VK_FALSE,
        VK_FALSE,
        m_config.polygonMode,
        m_config.cullMode,
        m_config.frontFace,
        VK_FALSE,
        0.0f, 0.0f, 0.0f,
        m_config.lineWidth);

    vk::PipelineMultisampleStateCreateInfo multisampling(
        {},
        m_config.samples,
        VK_FALSE,
        1.0f,
        nullptr,
        VK_FALSE,
        VK_FALSE);

    // ---- 颜色混合（单颜色附件）----
    vk::PipelineColorBlendAttachmentState colorBlendAttachment(
        m_config.blendEnable ? VK_TRUE : VK_FALSE,
        m_config.blendSrcColorFactor, m_config.blendDstColorFactor,
        vk::BlendOp::eAdd,
        m_config.blendSrcAlphaFactor, m_config.blendDstAlphaFactor,
        vk::BlendOp::eAdd,
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);

    vk::PipelineColorBlendStateCreateInfo colorBlending(
        {},
        VK_FALSE,
        vk::LogicOp::eCopy,
        1, &colorBlendAttachment,
        { 0.0f, 0.0f, 0.0f, 0.0f });

    // ---- 深度模板状态（可选）----
    const vk::PipelineDepthStencilStateCreateInfo depthStencil(
        {},
        m_config.depthTest  ? VK_TRUE : VK_FALSE,
        m_config.depthWrite ? VK_TRUE : VK_FALSE,
        m_config.depthCompareOp,
        VK_FALSE, VK_FALSE,
        {}, {},
        0.0f, 1.0f);
    const bool useDepth = m_config.depthTest || m_config.depthWrite;

    // ---- 管线布局：描述符集（可空）+ 推常量（可空）----
    std::vector<vk::DescriptorSetLayout> setLayouts;
    if (m_descriptorSetLayout != nullptr)
    {
        setLayouts.push_back(*m_descriptorSetLayout);
    }

    std::vector<vk::PushConstantRange> pushConstantRanges;
    if (m_config.pushConstant.size > 0 && bool(m_config.pushConstant.stageFlags))
    {
        pushConstantRanges.push_back(m_config.pushConstant);
    }

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo(
        {},
        setLayouts,
        pushConstantRanges);

    m_pipelineLayout = vk::raii::PipelineLayout(device.device(), pipelineLayoutInfo);

    vk::GraphicsPipelineCreateInfo pipelineInfo(
        {},
        shaderStages,
        &vertexInputInfo,
        &inputAssembly,
        nullptr,
        &viewportState,
        &rasterizer,
        &multisampling,
        useDepth ? &depthStencil : nullptr,
        &colorBlending,
        &dynamicStateInfo,
        *m_pipelineLayout,
        renderPass,
        0,
        nullptr,
        -1);

    m_pipeline = vk::raii::Pipeline(device.device(), nullptr, pipelineInfo);
    return m_pipeline != nullptr;
}

bool YalnGraphicsPipeline::create(const IYalnDevice&          device,
                                  const vk::raii::RenderPass& renderPass,
                                  const YalnPipelineConfig&   config)
{
    // 兼容路径：按配置中的路径内建 SPIR-V 着色器程序
    YalnSpirvShaderProgram program(config.vertShaderPath, config.fragShaderPath);
    if (!program.load(device))
    {
        return false;
    }
    return create(device, renderPass, config, program);
}

bool YalnGraphicsPipeline::create(const IYalnDevice&          device,
                                  const vk::raii::RenderPass& renderPass,
                                  const std::string&          vertPath,
                                  const std::string&          fragPath)
{
    YalnPipelineConfig config = YalnPipelineConfig::makeDefault();
    config.vertShaderPath = vertPath;
    config.fragShaderPath = fragPath;
    return create(device, renderPass, config);
}

void YalnGraphicsPipeline::destroy()
{
    m_pipeline            = nullptr;
    m_pipelineLayout      = nullptr;
    m_descriptorSetLayout = nullptr;
}
