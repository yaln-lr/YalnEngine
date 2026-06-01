// CPU端的网格数据
#ifndef YALN_MESH_H
#define YALN_MESH_H

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include "yalnvertex.h"
#include "global_type.h"

class YALN_CORE_EXPORT YalnMesh {
public:
    virtual ~YalnMesh(){};
    
    // 清空数据
    void clear();
    
    // 获取内存占用（字节）
    size_t getCPUMemorySize() const;
    
    // 检查是否有效
    bool isValid() const;
    
    // 计算包围盒
    void calculateBounds(glm::vec3& minBounds, glm::vec3& maxBounds) const;
    
    // 获取顶点和索引数据
    const std::vector<YalnVertexPC>& getVertices() const { return vertices; }
    const std::vector<uint32_t>& getIndices() const { return indices; }
    
    // 变换参数
    glm::vec3 m_position{0.0f, 0.0f, 0.0f};  // 位置偏移
    glm::vec3 m_rotation{0.0f, 0.0f, 0.0f};   // 旋转角度（弧度）
    glm::vec3 m_scale{1.0f, 1.0f, 1.0f};      // 缩放
    
    // 获取模型矩阵
    glm::mat4 getModelMatrix() const {
        glm::mat4 model = glm::identity<glm::mat4>();
        model = glm::translate(model, m_position);
        model = glm::rotate(model, m_rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::rotate(model, m_rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::rotate(model, m_rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
        model = glm::scale(model, m_scale);
        return model;
    }
    
protected:
    std::vector<YalnVertexPC> vertices;
    std::vector<uint32_t> indices;
};

// 立方体网格类
class YALN_CORE_EXPORT YalnCube : public YalnMesh
{
public:
    YalnCube(float l = 1.0f, float w = 1.0f, float h = 1.0f);
};

// 球体网格类
class YALN_CORE_EXPORT YalnSphere : public YalnMesh
{
public:
    YalnSphere(float radius = 0.5f, int segments = 32);
    
    float getRadius() const { return m_radius; }
    int getSegments() const { return m_segments; }
    
private:
    float m_radius;
    int m_segments;
};

#endif // YALN_MESH_H