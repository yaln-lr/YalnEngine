// CPU端的网格数据
#ifndef YALN_MESH_H
#define YALN_MESH_H

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <cstddef>
#include <string>
#include <vector>
#include "yalnvertex.h"
#include "global_type.h"

// ============================================================================
//  YalnMesh —— 网格抽象接口（纯虚，不含数据与实现）
//
//  变化点：不同场景的网格来源不同——程序化几何（Cube/Sphere）、文件加载
//  （obj/gltf）、程序生成（地形/噪声）、甚至 GPU 直供顶点流。
//  YalnScene / YalnRenderer 只依赖本接口，新增网格 = 新增派生类，上层零改动。
// ============================================================================
class YALN_CORE_EXPORT YalnMesh {
public:
    virtual ~YalnMesh() = default;

    // 顶点与索引（YalnScene 据此创建 GPU 缓冲）
    virtual const std::vector<YalnVertexPC>& getVertices() const = 0;
    virtual const std::vector<uint32_t>&     getIndices()  const = 0;

    // 通用顶点数据视图。默认把 getVertices() 当作 YalnVertexPC 数组；
    // 自定义顶点格式的网格可覆盖这三个方法，并在管线配置中声明对应布局。
    virtual const void* getVertexData() const
    {
        return static_cast<const void*>(getVertices().data());
    }
    virtual size_t getVertexStride() const { return sizeof(YalnVertexPC); }
    virtual size_t getVertexCount()  const { return getVertices().size(); }

    // 模型矩阵（YalnRenderer 每帧经 Push Constants 上传）
    virtual glm::mat4 getModelMatrix() const = 0;

    // 是否包含可渲染数据
    virtual bool isValid() const = 0;

    // CPU 端内存占用（字节）
    virtual size_t getCPUMemorySize() const = 0;

    // 局部空间包围盒；默认实现不支持（返回 false）。
    // 场景 update() 用它生成绘制项的世界空间包围盒，供后续剔除/排序使用。
    virtual bool getLocalBounds(glm::vec3& minBounds, glm::vec3& maxBounds) const
    {
        (void)minBounds;
        (void)maxBounds;
        return false;
    }
};

// ============================================================================
//  YalnStaticMesh —— 通用静态网格实现
//
//  CPU 端顶点/索引 + TRS 变换。绝大多数静态网格都可继承它，
//  子类只需在构造时填充 m_vertices / m_indices（如 YalnCube / YalnSphere）。
//  动态网格（每帧改顶点）或 GPU 侧网格可实现 YalnMesh 接口另起炉灶。
// ============================================================================
class YALN_CORE_EXPORT YalnStaticMesh : public YalnMesh {
public:
    YalnStaticMesh() = default;
    virtual ~YalnStaticMesh() = default;

    // ---- YalnMesh 接口实现 ----
    const std::vector<YalnVertexPC>& getVertices() const override { return m_vertices; }
    const std::vector<uint32_t>&     getIndices()  const override { return m_indices; }
    glm::mat4 getModelMatrix() const override;
    bool isValid() const override;
    size_t getCPUMemorySize() const override;
    bool getLocalBounds(glm::vec3& minBounds, glm::vec3& maxBounds) const override;

    // ---- 数据操作 ----
    void clear();
    void calculateBounds(glm::vec3& minBounds, glm::vec3& maxBounds) const;

    // 变换参数（保持原有公共字段用法）
    glm::vec3 m_position{0.0f, 0.0f, 0.0f};   // 位置偏移
    glm::vec3 m_rotation{0.0f, 0.0f, 0.0f};   // 旋转角度（弧度）
    glm::vec3 m_scale{1.0f, 1.0f, 1.0f};      // 缩放

protected:
    std::vector<YalnVertexPC> m_vertices;
    std::vector<uint32_t>     m_indices;
};

// 立方体网格类
class YALN_CORE_EXPORT YalnCube : public YalnStaticMesh
{
public:
    // 立方体网格。轴向约定（Z 轴向上）：l → X（长），w → Y（宽/深），h → Z（高）
    // color：实色填充；负分量（默认）表示使用经典彩虹顶点色
    YalnCube(float l = 1.0f, float w = 1.0f, float h = 1.0f,
             const glm::vec3& color = glm::vec3(-1.0f));
};

// 球体网格类
class YALN_CORE_EXPORT YalnSphere : public YalnStaticMesh
{
public:
    // color：实色填充；负分量（默认）表示使用法线渐变顶点色
    YalnSphere(float radius = 0.5f, int segments = 32,
               const glm::vec3& color = glm::vec3(-1.0f));

    float getRadius() const { return m_radius; }
    int getSegments() const { return m_segments; }

private:
    float m_radius;
    int m_segments;
};

// 四棱锥网格类（底面 w×l 居中于原点，顶点朝 +Z，整体沿 Z 居中）——适合做屋顶
class YALN_CORE_EXPORT YalnPyramid : public YalnStaticMesh
{
public:
    // color：实色填充；负分量（默认）表示使用经典暖色渐变顶点色
    YalnPyramid(float w = 1.0f, float l = 1.0f, float h = 1.0f,
                const glm::vec3& color = glm::vec3(-1.0f));
};

#endif // YALN_MESH_H
