#ifndef YALN_GRAPHICS_PIPELINE_H
#define YALN_GRAPHICS_PIPELINE_H

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <string>
#include <vector>

#include "global_type.h"

class YalnDevice;

// ============================================================================
//  YalnGraphicsPipeline
//  职责：着色器模块加载、描述符集布局、管线布局、图形管线本体。
//        视口/裁剪使用动态状态，因此窗口缩放不需要重建管线。
// ============================================================================
class YALN_CORE_EXPORT YalnGraphicsPipeline
{
public:
    YalnGraphicsPipeline() = default;

    YalnGraphicsPipeline(const YalnGraphicsPipeline&)            = delete;
    YalnGraphicsPipeline& operator=(const YalnGraphicsPipeline&) = delete;

    // vertPath / fragPath 为 SPIR-V 路径
    bool create(const YalnDevice&           device,
                const vk::raii::RenderPass& renderPass,
                const std::string&          vertPath,
                const std::string&          fragPath);

    void destroy();

    const vk::raii::DescriptorSetLayout& descriptorSetLayout() const { return m_descriptorSetLayout; }
    const vk::raii::PipelineLayout&      layout()              const { return m_pipelineLayout; }
    const vk::raii::Pipeline&            handle()              const { return m_pipeline; }

    bool isValid() const { return m_pipeline != nullptr; }

private:
    void createDescriptorSetLayout(const YalnDevice& device);
    vk::raii::ShaderModule createShaderModule(const YalnDevice& device, const std::vector<char>& code) const;

    static std::vector<char> readFile(const std::string& filename);

    vk::raii::DescriptorSetLayout m_descriptorSetLayout{ nullptr };
    vk::raii::PipelineLayout      m_pipelineLayout{ nullptr };
    vk::raii::Pipeline            m_pipeline{ nullptr };
};

#endif // YALN_GRAPHICS_PIPELINE_H
