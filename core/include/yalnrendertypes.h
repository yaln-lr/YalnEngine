#ifndef YALN_RENDER_TYPES_H
#define YALN_RENDER_TYPES_H

#include <cstddef>
#include <cstdint>
#include <ostream>

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
//  句柄是强类型值类型（零开销，底层为 uint32_t）：
//    - 默认构造 = 无效；显式构造 value() = 有效索引；
//    - 0 为默认项；invalid_value 表示无效；
//    - 不同 Tag 的句柄不能互相赋值/比较，编译期即拦截误用。
// ============================================================================

// 强类型句柄：编译后就是一个 uint32_t，零运行开销。
template <class Tag, class Value = std::uint32_t>
class YalnHandle
{
public:
    using value_type = Value;
    using tag_type   = Tag;

    static constexpr value_type invalid_value = static_cast<value_type>(-1);

    constexpr YalnHandle() noexcept = default;
    constexpr explicit YalnHandle(value_type v) noexcept : m_value(v) {}

    [[nodiscard]] constexpr bool valid() const noexcept
    {
        return m_value != invalid_value;
    }
    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
        return valid();
    }
    [[nodiscard]] constexpr value_type value() const noexcept
    {
        return m_value;
    }

    // C++17 不支持比较运算符的 = default，手写即可。
    [[nodiscard]] friend constexpr bool operator==(
        YalnHandle a, YalnHandle b) noexcept
    {
        return a.m_value == b.m_value;
    }
    [[nodiscard]] friend constexpr bool operator!=(
        YalnHandle a, YalnHandle b) noexcept
    {
        return !(a == b);
    }
    // 供排序使用：顺序稳定，且不依赖指针比较的未定义行为。
    [[nodiscard]] friend constexpr bool operator<(
        YalnHandle a, YalnHandle b) noexcept
    {
        return a.m_value < b.m_value;
    }

    friend std::ostream& operator<<(std::ostream& os, YalnHandle h)
    {
        return os << h.m_value;
    }

private:
    value_type m_value = invalid_value;
};

struct YalnPipelineTag;
struct YalnMaterialTag;

using YalnPipelineHandle = YalnHandle<YalnPipelineTag>;
using YalnMaterialHandle = YalnHandle<YalnMaterialTag>;

inline constexpr YalnPipelineHandle YALN_INVALID_PIPELINE{};
inline constexpr YalnMaterialHandle YALN_INVALID_MATERIAL{};
inline constexpr YalnPipelineHandle YALN_DEFAULT_PIPELINE{0u};
inline constexpr YalnMaterialHandle YALN_DEFAULT_MATERIAL{0u};

// 逻辑 Pass 的种类。作为位掩码使用：一个绘制项可同时参与多个 Pass。
enum class YalnPassKind : std::uint32_t
{
    Shadow      = 1u << 0,
    Opaque      = 1u << 1,
    Transparent = 1u << 2,
    Outline     = 1u << 3,
    PostProcess = 1u << 4
};

// 强类型 Pass 掩码：替代裸 uint32_t 位运算。
class YalnPassMask
{
public:
    using Bits = std::uint32_t;

    constexpr YalnPassMask() noexcept = default;
    constexpr explicit YalnPassMask(Bits bits) noexcept : m_bits(bits) {}

    [[nodiscard]] constexpr bool contains(YalnPassKind kind) const noexcept
    {
        return (m_bits & static_cast<Bits>(kind)) != 0;
    }
    [[nodiscard]] constexpr Bits bits() const noexcept { return m_bits; }

    [[nodiscard]] friend constexpr YalnPassMask operator|(
        YalnPassMask a, YalnPassMask b) noexcept
    {
        return YalnPassMask(a.m_bits | b.m_bits);
    }
    [[nodiscard]] friend constexpr YalnPassMask operator&(
        YalnPassMask a, YalnPassMask b) noexcept
    {
        return YalnPassMask(a.m_bits & b.m_bits);
    }
    [[nodiscard]] friend constexpr bool operator==(
        YalnPassMask a, YalnPassMask b) noexcept
    {
        return a.m_bits == b.m_bits;
    }
    [[nodiscard]] friend constexpr bool operator!=(
        YalnPassMask a, YalnPassMask b) noexcept
    {
        return !(a == b);
    }

private:
    Bits m_bits = 0;
};

inline constexpr YalnPassMask YALN_PASS_ALL{0xFFFFFFFFu};
// 兼容别名：旧代码使用 YALN_PASS_MASK_ALL
inline constexpr YalnPassMask YALN_PASS_MASK_ALL = YALN_PASS_ALL;

[[nodiscard]] constexpr YalnPassMask passMask(YalnPassKind kind) noexcept
{
    return YalnPassMask(static_cast<YalnPassMask::Bits>(kind));
}

// 兼容别名：旧代码里 toPassMask() 的调用点。
[[nodiscard]] constexpr YalnPassMask toPassMask(YalnPassKind kind) noexcept
{
    return passMask(kind);
}

// 材质内部按 Pass 存放管线句柄时使用的数组索引（0..4）。
constexpr std::uint32_t toPassIndex(YalnPassKind kind) noexcept
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

inline constexpr std::uint32_t YALN_PASS_SLOT_COUNT = 5u;

// 默认前向 Pass 的推常量布局。
// 必须与 shader/shaders/shader.vert / shader.frag 的 push_constant 块一致。
struct YALN_CORE_EXPORT YalnMaterialPushConstants
{
    glm::mat4 world{ 1.0f };
    glm::vec4 baseColor{ 1.0f, 1.0f, 1.0f, 1.0f };
};

// 把任意平凡可复制对象解释为字节序列（C++17；C++20 可用 std::as_bytes）。
template <class T>
[[nodiscard]] const std::byte* asBytes(const T& value) noexcept
{
    return reinterpret_cast<const std::byte*>(&value);
}

#endif // YALN_RENDER_TYPES_H
