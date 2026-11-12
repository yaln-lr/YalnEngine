#ifndef YALN_GRAPHICS_PIPELINE_H
#define YALN_GRAPHICS_PIPELINE_H

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <string>
#include <vector>

#include "global_type.h"
#include "yalninterfaces.h"

class YalnDevice;

// ============================================================================
//  YalnGraphicsPipeline —— IYalnPipeline 默认实现
//  职责：按配置创建描述符集布局、管线布局、图形管线本体。
//        着色器模块由 IYalnShaderProgram 提供（也可仅给路径，内部自建
//        YalnSpirvShaderProgram）。视口/裁剪使用动态状态，
//        窗口缩放不需要重建管线。
// ============================================================================
class YALN_CORE_EXPORT YalnGraphicsPipeline : public IYalnPipeline
{
public:
    YalnGraphicsPipeline() = default;

    YalnGraphicsPipeline(const YalnGraphicsPipeline&)            = delete;
    YalnGraphicsPipeline& operator=(const YalnGraphicsPipeline&) = delete;

    // 标准路径：着色器程序由调用方提供（可整体替换加载方式）
    bool create(const IYalnDevice&          device,
                const vk::raii::RenderPass& renderPass,
                const YalnPipelineConfig&   config,
                const IYalnShaderProgram&   shaderProgram) override;

    // 兼容路径：按 config.vertShaderPath/fragShaderPath 内部自建
    // YalnSpirvShaderProgram（与旧 vertPath/fragPath 重载等价）
    bool create(const IYalnDevice&          device,
                const vk::raii::RenderPass& renderPass,
                const YalnPipelineConfig&   config);

    // 兼容旧接口：仅指定两个着色器路径，其余使用默认配置
    bool create(const IYalnDevice&          device,
                const vk::raii::RenderPass& renderPass,
                const std::string&          vertPath,
                const std::string&          fragPath);

    void destroy() override;

    const vk::raii::DescriptorSetLayout& descriptorSetLayout() const override { return m_descriptorSetLayout; }
    const vk::raii::PipelineLayout&      layout()             const override { return m_pipelineLayout; }
    const vk::raii::Pipeline&            handle()             const override { return m_pipeline; }
    const YalnPipelineConfig&            config()             const override { return m_config; }

    // 供 YalnRenderer 判断是否需要创建/绑定描述符集与推常量
    bool hasDescriptorSets() const override { return m_descriptorSetLayout != nullptr; }
    bool hasPushConstants()  const override {
        return bool(m_config.pushConstant.stageFlags) && m_config.pushConstant.size > 0;
    }

    bool isValid() const override { return m_pipeline != nullptr; }

private:
    YalnPipelineConfig m_config;
    vk::raii::DescriptorSetLayout m_descriptorSetLayout{ nullptr };
    vk::raii::PipelineLayout      m_pipelineLayout{ nullptr };
    vk::raii::Pipeline            m_pipeline{ nullptr };
};

#endif // YALN_GRAPHICS_PIPELINE_H
