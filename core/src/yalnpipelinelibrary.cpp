#include "../include/yalnpipelinelibrary.h"
#include "../include/yalninterfaces.h"

#include <utility>

YalnPipelineLibrary::~YalnPipelineLibrary() = default;

namespace
{
std::size_t toIndex(YalnPipelineHandle handle) noexcept
{
    return static_cast<std::size_t>(handle.value());
}

YalnPipelineHandle makeHandle(std::size_t index) noexcept
{
    return YalnPipelineHandle(
        static_cast<YalnPipelineHandle::value_type>(index));
}
}  // namespace

YalnPipelineHandle YalnPipelineLibrary::addPipeline(
    std::unique_ptr<IYalnPipeline> pipeline, std::string name)
{
    if (pipeline == nullptr)
    {
        return YALN_INVALID_PIPELINE;
    }

    Entry entry;
    entry.owned = std::move(pipeline);
    entry.name  = std::move(name);

    const YalnPipelineHandle handle = makeHandle(m_entries.size());
    m_entries.push_back(std::move(entry));
    return handle;
}

YalnPipelineHandle YalnPipelineLibrary::addExternal(const IYalnPipeline& pipeline,
                                                     std::string name)
{
    Entry entry;
    entry.external = &pipeline;
    entry.name     = std::move(name);

    const YalnPipelineHandle handle = makeHandle(m_entries.size());
    m_entries.push_back(std::move(entry));
    return handle;
}

const IYalnPipeline* YalnPipelineLibrary::get(YalnPipelineHandle handle) const noexcept
{
    if (!handle.valid())
    {
        return nullptr;
    }
    const std::size_t index = toIndex(handle);
    if (index >= m_entries.size())
    {
        return nullptr;
    }

    const Entry& entry = m_entries[index];
    return entry.owned != nullptr ? entry.owned.get() : entry.external;
}

bool YalnPipelineLibrary::contains(YalnPipelineHandle handle) const noexcept
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
    if (!handle.valid())
    {
        return kEmpty;
    }
    const std::size_t index = toIndex(handle);
    if (index >= m_entries.size())
    {
        return kEmpty;
    }
    return m_entries[index].name;
}
