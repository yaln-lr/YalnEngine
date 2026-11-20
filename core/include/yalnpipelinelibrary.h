#ifndef YALN_PIPELINE_LIBRARY_H
#define YALN_PIPELINE_LIBRARY_H

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "global_type.h"
#include "yalnrendertypes.h"

class IYalnPipeline;

// ============================================================================
//  YalnPipelineLibrary —— PSO 注册表
//
//  一个 YalnPipelineHandle 对应一个 IYalnPipeline（Vulkan PSO）。
//  同一场景可以注册多条管线；材质按 Pass 引用其中一条。
//
//  所有权：
//    - addPipeline() 接管 unique_ptr，库负责销毁；
//    - addExternal() 只登记外部对象的地址，调用方必须保证生命周期覆盖库。
//      YalnView 的默认管线就是外部登记（句柄 0）。
//
//  句柄只在 clear() 前稳定；clear() 后旧句柄全部失效。
// ============================================================================
class YALN_CORE_EXPORT YalnPipelineLibrary
{
public:
    YalnPipelineLibrary() = default;
    ~YalnPipelineLibrary();

    YalnPipelineLibrary(const YalnPipelineLibrary&)            = delete;
    YalnPipelineLibrary& operator=(const YalnPipelineLibrary&) = delete;

    // 接管所有权，返回新句柄
    YalnPipelineHandle addPipeline(std::unique_ptr<IYalnPipeline> pipeline,
                                   std::string                     name = {});

    // 登记外部管线（不接管所有权），返回新句柄
    YalnPipelineHandle addExternal(const IYalnPipeline& pipeline,
                                   std::string           name = {});

    const IYalnPipeline* get(YalnPipelineHandle handle) const;
    bool                 isValid(YalnPipelineHandle handle) const;

    std::size_t size() const { return m_entries.size(); }
    void        clear();

    const std::string& name(YalnPipelineHandle handle) const;

private:
    struct Entry
    {
        std::unique_ptr<IYalnPipeline> owned;
        const IYalnPipeline*           external = nullptr;
        std::string                    name;
    };

    std::vector<Entry> m_entries;
};

#endif // YALN_PIPELINE_LIBRARY_H
