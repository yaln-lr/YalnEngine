#include "../include/yalnpipelinelibrary.h"
#include "../include/yalninterfaces.h"

#include <utility>

YalnPipelineLibrary::~YalnPipelineLibrary() = default;

YalnPipelineHandle YalnPipelineLibrary::addPipeline(std::unique_ptr<IYalnPipeline> pipeline,
                                                    std::string                     name)
{
    if (pipeline == nullptr)
    {
        return YALN_INVALID_PIPELINE;
    }

    Entry entry;
    entry.owned = std::move(pipeline);
    entry.name  = std::move(name);

    const auto handle = static_cast<YalnPipelineHandle>(m_entries.size());
    m_entries.push_back(std::move(entry));
    return handle;
}

YalnPipelineHandle YalnPipelineLibrary::addExternal(const IYalnPipeline& pipeline,
                                                    std::string           name)
{
    Entry entry;
    entry.external = &pipeline;
    entry.name     = std::move(name);

    const auto handle = static_cast<YalnPipelineHandle>(m_entries.size());
    m_entries.push_back(std::move(entry));
    return handle;
}

const IYalnPipeline* YalnPipelineLibrary::get(YalnPipelineHandle handle) const
{
    if (handle >= m_entries.size())
    {
        return nullptr;
    }

    const Entry& entry = m_entries[handle];
    return entry.owned != nullptr ? entry.owned.get() : entry.external;
}

bool YalnPipelineLibrary::isValid(YalnPipelineHandle handle) const
{
    return get(handle) != nullptr;
}

void YalnPipelineLibrary::clear()
{
    m_entries.clear();
}

const std::string& YalnPipelineLibrary::name(YalnPipelineHandle handle) const
{
    static const std::string kEmpty;
    if (handle >= m_entries.size())
    {
        return kEmpty;
    }
    return m_entries[handle].name;
}
