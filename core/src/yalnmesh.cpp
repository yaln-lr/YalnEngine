#include "../include/YalnMesh.h"
#include <cmath>

// 清空数据
void YalnMesh::clear() {
    vertices.clear();
    indices.clear();
    vertices.shrink_to_fit();
    indices.shrink_to_fit();
}

// 获取内存占用（字节）
size_t YalnMesh::getCPUMemorySize() const {
    return vertices.size() * sizeof(YalnVertexPC) + 
            indices.size() * sizeof(uint32_t);
}

// 检查是否有效
bool YalnMesh::isValid() const {
    return !vertices.empty() && !indices.empty();
}

// 计算包围盒
void YalnMesh::calculateBounds(glm::vec3& minBounds, glm::vec3& maxBounds) const {
    if (vertices.empty()) return;
    
    minBounds = vertices[0].pos;
    maxBounds = vertices[0].pos;
    
    for (const auto& vertex : vertices) {
        minBounds = glm::min(minBounds, vertex.pos);
        maxBounds = glm::max(maxBounds, vertex.pos);
    }
}

// 立方体构造函数
YalnCube::YalnCube(float l, float w, float h) {
    float hw = w * 0.5f;
    float hh = h * 0.5f;
    float hl = l * 0.5f;
    
    // 立方体的8个顶点 (位置 + 颜色)
    std::vector<YalnVertexPC> cubeVertices = {
        // 前面 (Z+)
        {{-hl, -hh,  hw}, {1.0f, 0.0f, 0.0f}},  // 0
        {{ hl, -hh,  hw}, {0.0f, 1.0f, 0.0f}},  // 1
        {{ hl,  hh,  hw}, {0.0f, 0.0f, 1.0f}},  // 2
        {{-hl,  hh,  hw}, {1.0f, 1.0f, 0.0f}},  // 3
        
        // 后面 (Z-)
        {{ hl, -hh, -hw}, {0.0f, 1.0f, 1.0f}},  // 4
        {{-hl, -hh, -hw}, {1.0f, 0.0f, 1.0f}},  // 5
        {{-hl,  hh, -hw}, {1.0f, 1.0f, 1.0f}},  // 6
        {{ hl,  hh, -hw}, {0.5f, 0.5f, 0.5f}}   // 7
    };
    
    // 12个三角形，36个索引
    std::vector<uint32_t> cubeIndices = {
        // 前面
        0, 1, 2,  2, 3, 0,
        // 右面
        1, 4, 7,  7, 2, 1,
        // 后面
        4, 5, 6,  6, 7, 4,
        // 左面
        5, 0, 3,  3, 6, 5,
        // 上面
        3, 2, 7,  7, 6, 3,
        // 下面
        5, 4, 1,  1, 0, 5
    };
    
    vertices = cubeVertices;
    indices = cubeIndices;
}

// 球体构造函数 (经纬度法)
YalnSphere::YalnSphere(float radius, int segments) 
    : m_radius(radius), m_segments(segments) {
    
    vertices.clear();
    indices.clear();
    
    for (int y = 0; y <= segments; y++) {
        float theta = glm::pi<float>() * y / segments;
        float sinTheta = std::sin(theta);
        float cosTheta = std::cos(theta);
        
        for (int x = 0; x <= segments; x++) {
            float phi = 2.0f * glm::pi<float>() * x / segments;
            float sinPhi = std::sin(phi);
            float cosPhi = std::cos(phi);
            
            // 计算位置
            glm::vec3 pos = glm::vec3(
                cosPhi * sinTheta,
                cosTheta,
                sinPhi * sinTheta
            ) * radius;
            
            // 计算法线 (= 归一化位置)
            glm::vec3 normal = glm::normalize(pos);
            
            // 颜色基于法线
            glm::vec3 color = (normal + 1.0f) * 0.5f;
            
            vertices.push_back({pos, color});
        }
    }
    
    // 生成索引
    for (int y = 0; y < segments; y++) {
        for (int x = 0; x < segments; x++) {
            int i0 = y * (segments + 1) + x;
            int i1 = i0 + 1;
            int i2 = (y + 1) * (segments + 1) + x;
            int i3 = i2 + 1;
            
            // 两个三角形组成一个四边形
            indices.push_back(i0);
            indices.push_back(i1);
            indices.push_back(i2);
            
            indices.push_back(i1);
            indices.push_back(i3);
            indices.push_back(i2);
        }
    }
}