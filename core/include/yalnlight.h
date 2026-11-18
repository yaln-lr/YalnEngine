#ifndef YALN_LIGHT_H
#define YALN_LIGHT_H

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>

#include "global_type.h"

// ============================================================================
//  灯光系统 —— 场景级光源（不挂在场景树上）
//
//  类型：Ambient（环境）/ Directional（平行）/ Point（点光）/ Spot（聚光）
//
//  数据路径：YalnLight（CPU）→ IYalnScene::lights() → 渲染器每帧打包进
//  YalnFrameUniform（std140）→ UBO → 片段着色器 Blinn-Phong 循环。
//
//  约定：
//    - 动态光源（平行/点/聚光）数量上限 YALN_MAX_LIGHTS（渲染器头文件定义），
//      超出部分被忽略并输出警告；环境光独立通道，最多生效 1 盏（后添加的覆盖）；
//    - gpuData() 是纯函数：方向归一化、锥角余弦换算等全部在 CPU 侧整理好，
//      着色器只做类型分支，不承担几何约定。
// ============================================================================

// 每盏灯在 GPU 上的表示（std140，48 字节，数组步长 48）
struct YALN_CORE_EXPORT YalnGPULight {
    alignas(16) glm::vec4 position;   // 语义随类型变化：
                                      //   平行光 xyz = 光线传播方向（单位向量）, w = 强度
                                      //   点光   xyz = 世界坐标,               w = 强度
                                      //   聚光   xyz = 世界坐标,               w = 强度
    alignas(16) glm::vec4 color;      // rgb = 光色（0~1）, a 未用
    alignas(16) glm::vec4 params;     // x = 作用半径（点光/聚光，<=0 视为无效）
                                      // y = 内锥角余弦（聚光）
                                      // z = 外锥角余弦（聚光）
                                      // w = 类型编号（1 平行 / 2 点光 / 3 聚光）
};

// 光源类型编号（与 shader.frag 中分支一致）
enum class YalnLightType {
    Ambient     = 0,
    Directional = 1,
    Point       = 2,
    Spot        = 3,
};

// 光源基类：颜色 + 强度
class YALN_CORE_EXPORT YalnLight
{
public:
    explicit YalnLight(YalnLightType type) : m_type(type) {}
    virtual ~YalnLight() = default;

    YalnLightType type() const { return m_type; }

    void setColor(const glm::vec3& color) { m_color = color; }
    const glm::vec3& color() const { return m_color; }

    void setIntensity(float intensity) { m_intensity = std::max(0.0f, intensity); }
    float intensity() const { return m_intensity; }

    // 打包为 GPU 表示（渲染器每帧调用）
    virtual YalnGPULight gpuData() const = 0;

protected:
    YalnLightType m_type;
    glm::vec3 m_color{1.0f};
    float m_intensity = 1.0f;
};

// ============================================================================
//  环境光：全场统一的底色照明，无方向、无衰减
//  典型用法：模拟天空散射，颜色偏冷、强度 0.4~0.7
// ============================================================================
class YALN_CORE_EXPORT YalnAmbientLight : public YalnLight
{
public:
    YalnAmbientLight() : YalnLight(YalnLightType::Ambient) {}

    YalnGPULight gpuData() const override
    {
        YalnGPULight gpu{};
        gpu.params.w = static_cast<float>(YalnLightType::Ambient);
        return gpu;
    }
};

// ============================================================================
//  平行光：无限远方向光（太阳/月光），无位置、无衰减
//  方向为"光的传播方向"；典型用法：主方向光强度 0.8~1.2
// ============================================================================
class YALN_CORE_EXPORT YalnDirectionalLight : public YalnLight
{
public:
    YalnDirectionalLight() : YalnLight(YalnLightType::Directional) {}

    // 方向自动归一化（零向量兜底为 -Z，避免着色器除零）
    void setDirection(const glm::vec3& direction)
    {
        const float len = glm::length(direction);
        m_direction = len > 1e-6f ? direction / len : glm::vec3(0.0f, 0.0f, -1.0f);
    }
    const glm::vec3& direction() const { return m_direction; }

    YalnGPULight gpuData() const override
    {
        YalnGPULight gpu{};
        gpu.position = glm::vec4(m_direction, m_intensity);
        gpu.color    = glm::vec4(m_color, 0.0f);
        gpu.params.w = static_cast<float>(YalnLightType::Directional);
        return gpu;
    }

private:
    glm::vec3 m_direction{0.0f, 0.0f, -1.0f};
};

// ============================================================================
//  点光源：向四周均匀发光，随距离平滑衰减
//  衰减模型：atten = (1 - d/range)^2（d = 片段到光源距离，range 外直接切除）
//  典型用法：路灯/火把，强度 1.0~2.0，半径 5~10
// ============================================================================
class YALN_CORE_EXPORT YalnPointLight : public YalnLight
{
public:
    YalnPointLight() : YalnLight(YalnLightType::Point) {}

    void setPosition(const glm::vec3& position) { m_position = position; }
    const glm::vec3& position() const { return m_position; }

    // 作用半径（<=0 视为无效光源）
    void setRange(float range) { m_range = std::max(0.0f, range); }
    float range() const { return m_range; }

    YalnGPULight gpuData() const override
    {
        YalnGPULight gpu{};
        gpu.position = glm::vec4(m_position, m_intensity);
        gpu.color    = glm::vec4(m_color, 0.0f);
        gpu.params   = glm::vec4(m_range, 0.0f, 0.0f, static_cast<float>(YalnLightType::Point));
        return gpu;
    }

private:
    glm::vec3 m_position{0.0f};
    float m_range = 10.0f;
};

// ============================================================================
//  聚光灯：点光源 + 方向锥，内外锥角之间平滑过渡
//  锥角用"半角"指定（度）：内锥内全亮，内锥→外锥平滑衰减，外锥外熄灭
//  典型用法：探照灯/手电/门廊灯，强度 1.5~2.5
// ============================================================================
class YALN_CORE_EXPORT YalnSpotLight : public YalnLight
{
public:
    YalnSpotLight() : YalnLight(YalnLightType::Spot) {}

    void setPosition(const glm::vec3& position) { m_position = position; }
    const glm::vec3& position() const { return m_position; }

    // 照射方向（光的传播方向），自动归一化（零向量兜底为 -Z）
    void setDirection(const glm::vec3& direction)
    {
        const float len = glm::length(direction);
        m_direction = len > 1e-6f ? direction / len : glm::vec3(0.0f, 0.0f, -1.0f);
    }
    const glm::vec3& direction() const { return m_direction; }

    // 一次设置内外锥半角（度）；自动保证 inner <= outer
    void setAngles(float innerDegrees, float outerDegrees)
    {
        outerDegrees = std::clamp(outerDegrees, 0.5f, 179.0f);
        innerDegrees = std::clamp(innerDegrees, 0.0f, outerDegrees);
        m_innerCos = std::cos(glm::radians(innerDegrees));
        m_outerCos = std::cos(glm::radians(outerDegrees));
    }

    float innerAngleCos() const { return m_innerCos; }
    float outerAngleCos() const { return m_outerCos; }

    // 作用半径（<=0 视为无效光源）
    void setRange(float range) { m_range = std::max(0.0f, range); }
    float range() const { return m_range; }

    YalnGPULight gpuData() const override
    {
        YalnGPULight gpu{};
        gpu.position = glm::vec4(m_position, m_intensity);
        gpu.color    = glm::vec4(m_color, 0.0f);
        gpu.params   = glm::vec4(m_range, m_innerCos, m_outerCos,
                                 static_cast<float>(YalnLightType::Spot));
        return gpu;
    }

private:
    glm::vec3 m_position{0.0f};
    glm::vec3 m_direction{0.0f, 0.0f, -1.0f};
    float m_range    = 15.0f;
    float m_innerCos = 0.90f;   // 默认约 26° 内锥
    float m_outerCos = 0.75f;   // 默认约 41° 外锥
};

#endif // YALN_LIGHT_H
