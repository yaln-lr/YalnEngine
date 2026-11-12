#ifndef YALN_SHADER_H
#define YALN_SHADER_H

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <string>

#include "global_type.h"
#include "yalninterfaces.h"

// ============================================================================
//  YalnSpirvShader —— IYalnShader 默认实现
//
//  从 SPIR-V 文件加载单个着色器阶段。一个实例 = 一个阶段。
//
//  扩展点：派生可替换加载来源（内存嵌入、GLSL 运行时编译、热重载等），
//  或实现特殊阶段（如按材质切换的多入口着色器）。
// ============================================================================
class YALN_CORE_EXPORT YalnSpirvShader : public IYalnShader
{
public:
    YalnSpirvShader(YalnShaderStage stage,
                    std::string     path,
                    std::string     entryPoint = "main");

    ~YalnSpirvShader() override = default;

    YalnSpirvShader(const YalnSpirvShader&)            = delete;
    YalnSpirvShader& operator=(const YalnSpirvShader&) = delete;

    YalnShaderStage stage() const override { return m_stage; }
    std::string entryPoint() const override { return m_entryPoint; }

    bool load(const vk::raii::Device& device) override;
    void unload() override;
    bool isValid() const override { return m_module != nullptr; }

    vk::PipelineShaderStageCreateInfo stageInfo() const override;

    const std::string& path() const { return m_path; }

private:
    static std::vector<char> readFile(const std::string& filename);

    YalnShaderStage m_stage;
    std::string     m_path;
    std::string     m_entryPoint;

    vk::raii::ShaderModule m_module{ nullptr };
};

#endif // YALN_SHADER_H
