#ifndef YALN_SHADER_PROGRAM_H
#define YALN_SHADER_PROGRAM_H

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <string>
#include <vector>

#include "global_type.h"
#include "yalninterfaces.h"

// ============================================================================
//  YalnSpirvShaderProgram —— 着色器程序默认实现
//
//  着色器集合：按需添加任意阶段的 YalnSpirvShader（或自定义 IYalnShader），
//  load() 时统一校验组合规则（必须阶段 / 成对 / 设备特性）。
//
//  与 YalnPipelineConfig 的路径字段配套的兼容构造保留：
//  管线不传程序时按 config.vertShaderPath/fragShaderPath 自行构建本实现。
// ============================================================================
class YALN_CORE_EXPORT YalnSpirvShaderProgram : public IYalnShaderProgram
{
public:
    // 兼容构造：顶点 + 片段（最常见的图形程序）
    YalnSpirvShaderProgram(std::string vertPath,
                           std::string fragPath,
                           std::string entryPoint = "main");

    explicit YalnSpirvShaderProgram(const YalnShaderProgramConfig& config);

    ~YalnSpirvShaderProgram() override = default;

    YalnSpirvShaderProgram(const YalnSpirvShaderProgram&)            = delete;
    YalnSpirvShaderProgram& operator=(const YalnSpirvShaderProgram&) = delete;

    // ---- IYalnShaderProgram ----
    bool addShader(std::unique_ptr<IYalnShader> shader) override;
    bool load(const IYalnDevice& device) override;
    void unload() override;
    bool isValid() const override;
    Type  type()  const override { return m_type; }
    const std::vector<std::unique_ptr<IYalnShader>>& shaders() const override { return m_shaders; }
    std::vector<vk::PipelineShaderStageCreateInfo> stageInfos() const override;

private:
    // 组合规则校验：返回 false 时 problems 给出全部原因（含设备特性检查）
    bool validateComposition(const IYalnDevice& device, std::vector<std::string>& problems);

    std::vector<std::unique_ptr<IYalnShader>> m_shaders;
    Type m_type   = Type::Empty;
    bool m_loaded = false;
};

#endif // YALN_SHADER_PROGRAM_H
