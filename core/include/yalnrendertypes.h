#ifndef YALN_RENDER_TYPES_H
#define YALN_RENDER_TYPES_H

#include <cstdint>

#include <glm/glm.hpp>

#include "global_type.h"

// ============================================================================
//  渲染层共享类型
//
//  重要区分：
//    - YalnPipelineHandle 指向「Vulkan PSO（IYalnPipeline）」；
//    - YalnMaterialHandle 指向「材质参数集合（YalnMaterial）」；
//    - YalnFramePipeline 是整帧策略，不是这里的 PSO。
//
//  句柄是库内稳定索引：0 为默认项，0xFFFFFFFF 表示无效。
// ============================================================================

using YalnPipelineHandle = uint32_t;
using YalnMaterialHandle = uint32_t;

inline constexpr YalnPipelineHandle YALN_INVALID_PIPELINE = 0xFFFFFFFFu;
inline constexpr YalnMaterialHandle YALN_INVALID_MATERIAL = 0xFFFFFFFFu;

inline constexpr YalnPipelineHandle YALN_DEFAULT_PIPELINE = 0u;
inline constexpr YalnMaterialHandle YALN_DEFAULT_MATERIAL = 0u;

// 逻辑 Pass 的种类。作为位掩码使用：一个绘制项可同时参与多个 Pass。
enum class YalnPassKind : uint32_t
{
    Shadow      = 1u << 0,
    Opaque      = 1u << 1,
    Transparent = 1u << 2,
    Outline     = 1u << 3,
    PostProcess = 1u << 4
};

inline constexpr uint32_t YALN_PASS_MASK_ALL = 0xFFFFFFFFu;

constexpr uint32_t toPassMask(YalnPassKind kind)
{
    return static_cast<uint32_t>(kind);
}

// 材质内部按 Pass 存放管线句柄时使用的数组索引（0..4）。
constexpr uint32_t toPassIndex(YalnPassKind kind)
{
    switch (kind)
    {
        case YalnPassKind::Shadow:      return 0u;
        case YalnPassKind::Opaque:      return 1u;
        case YalnPassKind::Transparent: return 2u;
        case YalnPassKind::Outline:     return 3u;
        case YalnPassKind::PostProcess: return 4u;
    }
    return 0u;
}

inline constexpr uint32_t YALN_PASS_SLOT_COUNT = 5u;

// 默认前向 Pass 的推常量布局。
// 必须与 shader/shaders/shader.vert / shader.frag 的 push_constant 块一致。
struct YALN_CORE_EXPORT YalnMaterialPushConstants
{
    glm::mat4 world{ 1.0f };
    glm::vec4 baseColor{ 1.0f, 1.0f, 1.0f, 1.0f };
};

#endif // YALN_RENDER_TYPES_H
