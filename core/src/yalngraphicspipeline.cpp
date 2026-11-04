#include "../include/yalngraphicspipeline.h"
#include "../include/yalndevice.h"
#include "../include/yalnvertex.h"

#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>

std::vector<char> YalnGraphicsPipeline::readFile(const std::string& filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        throw std::runtime_error("failed to open file: " + filename);
    }

    const size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
    file.close();
    return buffer;
}

vk::raii::ShaderModule YalnGraphicsPipeline::createShaderModule(const YalnDevice& device, const std::vector<char>& code) const
{
    vk::ShaderModuleCreateInfo createInfo(
        {},
        static_cast<uint32_t>(code.size()),
        reinterpret_cast<const uint32_t*>(code.data()));

    return vk::raii::ShaderModule(device.device(), createInfo);
}

void YalnGraphicsPipeline::createDescriptorSetLayout(const YalnDevice& device)
{
    vk::DescriptorSetLayoutBinding uboLayoutBinding(
        0,
        vk::DescriptorType::eUniformBuffer,
        1,
        vk::ShaderStageFlagBits::eVertex,
        nullptr);

    vk::DescriptorSetLayoutCreateInfo layoutInfo({}, 1, &uboLayoutBinding);
    m_descriptorSetLayout = vk::raii::DescriptorSetLayout(device.device(), layoutInfo);
}

bool YalnGraphicsPipeline::create(const YalnDevice&           device,
                                  const vk::raii::RenderPass& renderPass,
                                  const std::string&          vertPath,
                                  const std::string&          fragPath)
{
    createDescriptorSetLayout(device);

    std::vector<char> vertShaderCode;
    std::vector<char> fragShaderCode;
    try
    {
        vertShaderCode = readFile(vertPath);
        fragShaderCode = readFile(fragPath);
    }
    catch (const std::exception& e)
    {
        std::cerr << "[YalnGraphicsPipeline] " << e.what() << std::endl;
        return false;
    }

    const auto vertShaderModule = createShaderModule(device, vertShaderCode);
    const auto fragShaderModule = createShaderModule(device, fragShaderCode);

    vk::PipelineShaderStageCreateInfo vertShaderStageInfo({}, vk::ShaderStageFlagBits::eVertex,   *vertShaderModule, "main");
    vk::PipelineShaderStageCreateInfo fragShaderStageInfo({}, vk::ShaderStageFlagBits::eFragment, *fragShaderModule, "main");

    const std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages = {
        vertShaderStageInfo, fragShaderStageInfo
    };

    // 顶点输入
    const auto bindingDescription    = YalnVertexPC::getBindingDescription();
    const auto attributeDescriptions = YalnVertexPC::getAttributeDescriptions();

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo(
        {},
        1, reinterpret_cast<const vk::VertexInputBindingDescription*>(&bindingDescription),
        static_cast<uint32_t>(attributeDescriptions.size()),
        reinterpret_cast<const vk::VertexInputAttributeDescription*>(attributeDescriptions.data()));

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly(
        {},
        vk::PrimitiveTopology::eTriangleList,
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
        vk::PolygonMode::eFill,
        vk::CullModeFlagBits::eBack,
        vk::FrontFace::eCounterClockwise,
        VK_FALSE,
        0.0f, 0.0f, 0.0f,
        1.0f);

    vk::PipelineMultisampleStateCreateInfo multisampling(
        {},
        vk::SampleCountFlagBits::e1,
        VK_FALSE,
        1.0f,
        nullptr,
        VK_FALSE,
        VK_FALSE);

    vk::PipelineColorBlendAttachmentState colorBlendAttachment(
        VK_FALSE,
        vk::BlendFactor::eZero, vk::BlendFactor::eOne,
        vk::BlendOp::eAdd,
        vk::BlendFactor::eZero, vk::BlendFactor::eOne,
        vk::BlendOp::eAdd,
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);

    vk::PipelineColorBlendStateCreateInfo colorBlending(
        {},
        VK_FALSE,
        vk::LogicOp::eCopy,
        1, &colorBlendAttachment,
        { 0.0f, 0.0f, 0.0f, 0.0f });

    // Push Constants：每个网格的模型矩阵
    const vk::PushConstantRange pushConstantRange(vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4));
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo(
        {},
        1, &*m_descriptorSetLayout,
        1, &pushConstantRange);

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
        nullptr,
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

void YalnGraphicsPipeline::destroy()
{
    m_pipeline            = nullptr;
    m_pipelineLayout      = nullptr;
    m_descriptorSetLayout = nullptr;
}
