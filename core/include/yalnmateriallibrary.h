#ifndef YALN_MATERIAL_LIBRARY_H
#define YALN_MATERIAL_LIBRARY_H

#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "global_type.h"
#include "yalnrendertypes.h"

// ============================================================================
//  YalnMaterialDesc / YalnMaterial —— 材质描述与实例
//
//  材质不是 PSO：它引用管线（可按 Pass 分别引用），并携带材质参数。
//  当前默认前向 Pass 使用 baseColor 作为乘色；后续可扩展贴图、
//  粗糙度、金属度、描述符集等参数而不改变绘制项结构。
//
//  passPipelines 按 YalnPassKind 的固定槽位保存：
//    [0]=Shadow [1]=Opaque [2]=Transparent [3]=Outline [4]=PostProcess
//  某个槽位为无效句柄时回退到 defaultPipeline。
// ============================================================================
struct YALN_CORE_EXPORT YalnMaterialDesc
{
    std::string name = "Material";

    glm::vec4 baseColor{ 1.0f, 1.0f, 1.0f, 1.0f };
    uint32_t  passMask = YALN_PASS_MASK_ALL;

    YalnPipelineHandle defaultPipeline = YALN_DEFAULT_PIPELINE;

    std::array<YalnPipelineHandle, YALN_PASS_SLOT_COUNT> passPipelines = {
        YALN_INVALID_PIPELINE,
        YALN_INVALID_PIPELINE,
        YALN_INVALID_PIPELINE,
        YALN_INVALID_PIPELINE,
        YALN_INVALID_PIPELINE
    };

    // 为某个 Pass 指定专用管线（例如 Opaque 用 PBR、Shadow 用深度管线）
    void setPipeline(YalnPassKind pass, YalnPipelineHandle pipeline);
};

class YALN_CORE_EXPORT YalnMaterial
{
public:
    explicit YalnMaterial(YalnMaterialDesc desc);

    const YalnMaterialDesc& desc() const { return m_desc; }
    const glm::vec4&        baseColor() const { return m_desc.baseColor; }
    uint32_t                passMask() const { return m_desc.passMask; }

    // 运行期更新材质参数（下一帧生效；当前仅 baseColor）
    void setBaseColor(const glm::vec4& color) { m_desc.baseColor = color; }

    YalnPipelineHandle pipelineForPass(YalnPassKind pass) const;

private:
    YalnMaterialDesc m_desc;
};

// ============================================================================
//  YalnMaterialLibrary —— 材质注册表
//
//  句柄 0 为默认材质（ensureDefault() 创建），默认材质使用默认管线、
//  白色乘色、参与全部 Pass。所有未显式指定材质的绘制项都落到句柄 0。
// ============================================================================
class YALN_CORE_EXPORT YalnMaterialLibrary
{
public:
    YalnMaterialLibrary() = default;
    ~YalnMaterialLibrary();

    YalnMaterialLibrary(const YalnMaterialLibrary&)            = delete;
    YalnMaterialLibrary& operator=(const YalnMaterialLibrary&) = delete;

    YalnMaterialHandle       createMaterial(YalnMaterialDesc desc);
    YalnMaterial*            get(YalnMaterialHandle handle);
    const YalnMaterial*      get(YalnMaterialHandle handle) const;
    bool                     isValid(YalnMaterialHandle handle) const;

    std::size_t size() const { return m_materials.size(); }
    void        clear();

    // 库为空时创建默认材质并返回 0；库非空时返回 0（要求 0 已是默认材质）
    YalnMaterialHandle ensureDefault(YalnPipelineHandle defaultPipeline = YALN_DEFAULT_PIPELINE);

private:
    std::vector<std::unique_ptr<YalnMaterial>> m_materials;
};

#endif // YALN_MATERIAL_LIBRARY_H
