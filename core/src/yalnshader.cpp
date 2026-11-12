#include "../include/yalnshader.h"

#include <fstream>
#include <iostream>
#include <stdexcept>

YalnSpirvShader::YalnSpirvShader(YalnShaderStage stage,
                                 std::string     path,
                                 std::string     entryPoint)
    : m_stage(stage)
    , m_path(std::move(path))
    , m_entryPoint(std::move(entryPoint))
{
}

bool YalnSpirvShader::load(const vk::raii::Device& device)
{
    if (isValid())
    {
        unload();
    }

    std::vector<char> code;
    try
    {
        code = readFile(m_path);
    }
    catch (const std::exception& e)
    {
        std::cerr << "[YalnSpirvShader:" << toString(m_stage) << "] " << e.what() << std::endl;
        return false;
    }

    vk::ShaderModuleCreateInfo createInfo(
        {},
        static_cast<uint32_t>(code.size()),
        reinterpret_cast<const uint32_t*>(code.data()));

    m_module = vk::raii::ShaderModule(device, createInfo);
    return isValid();
}

void YalnSpirvShader::unload()
{
    m_module = nullptr;
}

vk::PipelineShaderStageCreateInfo YalnSpirvShader::stageInfo() const
{
    // 调用方须在本对象存活期间使用返回的 stage 信息（其引用模块句柄）
    return vk::PipelineShaderStageCreateInfo(
        {}, toVulkanShaderStage(m_stage), *m_module, m_entryPoint.c_str());
}

std::vector<char> YalnSpirvShader::readFile(const std::string& filename)
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
