#include "../include/yalnshaderprogram.h"
#include "../include/yalnshader.h"
#include "../include/yalninterfaces.h"

#include <iostream>
#include <map>
#include <set>

// 兼容构造：顶点 + 片段
YalnSpirvShaderProgram::YalnSpirvShaderProgram(std::string vertPath,
                                               std::string fragPath,
                                               std::string entryPoint)
{
    addShader(std::make_unique<YalnSpirvShader>(YalnShaderStage::Vertex,
                                                std::move(vertPath), entryPoint));
    addShader(std::make_unique<YalnSpirvShader>(YalnShaderStage::Fragment,
                                                std::move(fragPath), entryPoint));
}

YalnSpirvShaderProgram::YalnSpirvShaderProgram(const YalnShaderProgramConfig& config)
    : YalnSpirvShaderProgram(config.vertPath, config.fragPath, config.entryPoint)
{
}

bool YalnSpirvShaderProgram::addShader(std::unique_ptr<IYalnShader> shader)
{
    if (shader == nullptr)
    {
        std::cerr << "[YalnSpirvShaderProgram] addShader: 不能添加空着色器" << std::endl;
        return false;
    }

    if (m_loaded)
    {
        std::cerr << "[YalnSpirvShaderProgram] 程序已加载，添加新着色器后需重新 load()" << std::endl;
        return false;
    }

    // 同一阶段不得重复
    for (const auto& existing : m_shaders)
    {
        if (existing->stage() == shader->stage())
        {
            std::cerr << "[YalnSpirvShaderProgram] 阶段 " << toString(shader->stage())
                      << " 已存在，不能重复添加" << std::endl;
            return false;
        }
    }

    m_shaders.push_back(std::move(shader));
    return true;
}

bool YalnSpirvShaderProgram::load(const IYalnDevice& device)
{
    if (m_loaded)
    {
        unload();
    }

    // ---- 组合规则与设备特性校验（在加载前先报告全部问题）----
    std::vector<std::string> problems;
    if (!validateComposition(device, problems))
    {
        for (const std::string& problem : problems)
        {
            std::cerr << "[YalnSpirvShaderProgram] 校验失败: " << problem << std::endl;
        }
        return false;
    }

    // ---- 逐个加载 ----
    for (auto& shader : m_shaders)
    {
        if (!shader->load(device.device()))
        {
            unload();   // 部分成功也算失败，整体回滚
            return false;
        }
    }

    m_loaded = true;
    return true;
}

void YalnSpirvShaderProgram::unload()
{
    for (auto& shader : m_shaders)
    {
        shader->unload();
    }
    m_loaded = false;
}

bool YalnSpirvShaderProgram::isValid() const
{
    if (!m_loaded || m_shaders.empty())
    {
        return false;
    }
    for (const auto& shader : m_shaders)
    {
        if (!shader->isValid())
        {
            return false;
        }
    }
    return true;
}

std::vector<vk::PipelineShaderStageCreateInfo> YalnSpirvShaderProgram::stageInfos() const
{
    // 调用方须在本对象存活期间使用返回的 stage 信息（其引用模块句柄）
    std::vector<vk::PipelineShaderStageCreateInfo> infos;
    infos.reserve(m_shaders.size());
    for (const auto& shader : m_shaders)
    {
        infos.push_back(shader->stageInfo());
    }
    return infos;
}

bool YalnSpirvShaderProgram::validateComposition(const IYalnDevice& device,
                                                 std::vector<std::string>& problems)
{
    if (m_shaders.empty())
    {
        problems.push_back("程序为空：至少需要一个着色器阶段");
        return false;
    }

    // ---- 按阶段分类 ----
    std::set<YalnShaderStage> stages;
    for (const auto& shader : m_shaders)
    {
        stages.insert(shader->stage());
    }

    const bool isCompute   = stages.count(YalnShaderStage::Compute) > 0;
    const bool hasGraphics = stages.count(YalnShaderStage::Vertex) > 0
                          || stages.count(YalnShaderStage::Fragment) > 0
                          || stages.count(YalnShaderStage::TessControl) > 0
                          || stages.count(YalnShaderStage::TessEvaluation) > 0
                          || stages.count(YalnShaderStage::Geometry) > 0;

    // ---- 类型推断与混入检查 ----
    if (isCompute && hasGraphics)
    {
        problems.push_back("组合错误：计算阶段不得与图形阶段混入同一程序");
    }
    m_type = isCompute && !hasGraphics ? Type::Compute
           : hasGraphics && !isCompute ? Type::Graphics
           : Type::Mixed;
    if (m_type == Type::Mixed)
    {
        return false;
    }

    // ---- 图形程序规则 ----
    if (m_type == Type::Graphics)
    {
        if (stages.count(YalnShaderStage::Vertex) == 0)
        {
            problems.push_back("图形程序缺少必须实现的 Vertex 阶段");
        }
        if (stages.count(YalnShaderStage::Fragment) == 0)
        {
            problems.push_back("图形程序缺少必须实现的 Fragment 阶段");
        }
        const bool hasTessCtrl = stages.count(YalnShaderStage::TessControl) > 0;
        const bool hasTessEval = stages.count(YalnShaderStage::TessEvaluation) > 0;
        if (hasTessCtrl != hasTessEval)
        {
            problems.push_back("曲面细分控制与求值着色器必须成对出现");
        }
    }
    // ---- 计算程序规则 ----
    else if (m_type == Type::Compute)
    {
        if (stages.count(YalnShaderStage::Compute) == 0)
        {
            problems.push_back("计算程序缺少必须实现的 Compute 阶段");
        }
    }

    // ---- 设备特性 ----
    const vk::PhysicalDeviceFeatures features = device.physicalDevice().getFeatures();
    if ((stages.count(YalnShaderStage::TessControl) > 0
         || stages.count(YalnShaderStage::TessEvaluation) > 0)
        && !features.tessellationShader)
    {
        problems.push_back("设备不支持 tessellationShader 特性，无法使用曲面细分阶段");
    }
    if (stages.count(YalnShaderStage::Geometry) > 0 && !features.geometryShader)
    {
        problems.push_back("设备不支持 geometryShader 特性，无法使用几何着色器阶段");
    }

    return problems.empty();
}
