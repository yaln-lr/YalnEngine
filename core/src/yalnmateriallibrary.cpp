#include "../include/yalnmateriallibrary.h"

#include <utility>

void YalnMaterialDesc::setPipeline(YalnPassKind pass, YalnPipelineHandle pipeline)
{
    const uint32_t index = toPassIndex(pass);
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
    const uint32_t index = toPassIndex(pass);
    if (index < m_desc.passPipelines.size())
    {
        const YalnPipelineHandle specific = m_desc.passPipelines[index];
        if (specific != YALN_INVALID_PIPELINE)
        {
            return specific;
        }
    }
    return m_desc.defaultPipeline;
}

YalnMaterialLibrary::~YalnMaterialLibrary() = default;

YalnMaterialHandle YalnMaterialLibrary::createMaterial(YalnMaterialDesc desc)
{
    auto material = std::make_unique<YalnMaterial>(std::move(desc));

    const auto handle = static_cast<YalnMaterialHandle>(m_materials.size());
    m_materials.push_back(std::move(material));
    return handle;
}

YalnMaterial* YalnMaterialLibrary::get(YalnMaterialHandle handle)
{
    if (handle >= m_materials.size())
    {
        return nullptr;
    }
    return m_materials[handle].get();
}

const YalnMaterial* YalnMaterialLibrary::get(YalnMaterialHandle handle) const
{
    if (handle >= m_materials.size())
    {
        return nullptr;
    }
    return m_materials[handle].get();
}

bool YalnMaterialLibrary::isValid(YalnMaterialHandle handle) const
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
    desc.passMask        = YALN_PASS_MASK_ALL;
    desc.defaultPipeline = defaultPipeline;

    return createMaterial(std::move(desc));
}
