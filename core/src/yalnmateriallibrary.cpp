#include "../include/yalnmateriallibrary.h"

#include <utility>

void YalnMaterialDesc::setPipeline(YalnPassKind pass, YalnPipelineHandle pipeline)
{
    const std::uint32_t index = toPassIndex(pass);
    if (index < passPipelines.size())
    {
        passPipelines[index] = pipeline;
    }
}

YalnMaterial::YalnMaterial(YalnMaterialDesc desc)
    : m_desc(std::move(desc))
{
}

YalnPipelineHandle YalnMaterial::pipelineForPass(YalnPassKind pass) const
{
    const std::uint32_t index = toPassIndex(pass);
    if (index < m_desc.passPipelines.size())
    {
        const YalnPipelineHandle specific = m_desc.passPipelines[index];
        if (specific.valid())
        {
            return specific;
        }
    }
    return m_desc.defaultPipeline;
}

YalnMaterialLibrary::~YalnMaterialLibrary() = default;

namespace
{
std::size_t toIndex(YalnMaterialHandle handle) noexcept
{
    return static_cast<std::size_t>(handle.value());
}
}  // namespace

YalnMaterialHandle YalnMaterialLibrary::createMaterial(YalnMaterialDesc desc)
{
    auto material = std::make_unique<YalnMaterial>(std::move(desc));

    const auto handle = YalnMaterialHandle(
        static_cast<YalnMaterialHandle::value_type>(m_materials.size()));
    m_materials.push_back(std::move(material));
    return handle;
}

YalnMaterial* YalnMaterialLibrary::get(YalnMaterialHandle handle) noexcept
{
    if (!handle.valid())
    {
        return nullptr;
    }
    const std::size_t index = toIndex(handle);
    return index < m_materials.size() ? m_materials[index].get() : nullptr;
}

const YalnMaterial* YalnMaterialLibrary::get(YalnMaterialHandle handle) const noexcept
{
    if (!handle.valid())
    {
        return nullptr;
    }
    const std::size_t index = toIndex(handle);
    return index < m_materials.size() ? m_materials[index].get() : nullptr;
}

bool YalnMaterialLibrary::contains(YalnMaterialHandle handle) const noexcept
{
    return get(handle) != nullptr;
}

void YalnMaterialLibrary::clear()
{
    m_materials.clear();
}

YalnMaterialHandle YalnMaterialLibrary::ensureDefault(YalnPipelineHandle defaultPipeline)
{
    if (!m_materials.empty())
    {
        return YALN_DEFAULT_MATERIAL;
    }

    YalnMaterialDesc desc;
    desc.name            = "Default";
    desc.baseColor       = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    desc.passMask        = YALN_PASS_ALL;
    desc.defaultPipeline = defaultPipeline;

    return createMaterial(std::move(desc));
}
