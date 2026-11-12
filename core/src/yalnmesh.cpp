#include "../include/YalnMesh.h"
#include <cmath>

// ==================== YalnStaticMesh：通用静态网格实现 ====================

glm::mat4 YalnStaticMesh::getModelMatrix() const {
    glm::mat4 model = glm::identity<glm::mat4>();
    model = glm::translate(model, m_position);
    model = glm::rotate(model, m_rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, m_rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, m_rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, m_scale);
    return model;
}

// 检查是否有效
bool YalnStaticMesh::isValid() const {
    return !m_vertices.empty() && !m_indices.empty();
}

// 获取内存占用（字节）
size_t YalnStaticMesh::getCPUMemorySize() const {
    return m_vertices.size() * sizeof(YalnVertexPC) +
            m_indices.size() * sizeof(uint32_t);
}

// 清空数据
void YalnStaticMesh::clear() {
    m_vertices.clear();
    m_indices.clear();
    m_vertices.shrink_to_fit();
    m_indices.shrink_to_fit();
}

// 计算包围盒
void YalnStaticMesh::calculateBounds(glm::vec3& minBounds, glm::vec3& maxBounds) const {
    if (m_vertices.empty()) return;

    minBounds = m_vertices[0].pos;
    maxBounds = m_vertices[0].pos;

    for (const auto& vertex : m_vertices) {
        minBounds = glm::min(minBounds, vertex.pos);
        maxBounds = glm::max(maxBounds, vertex.pos);
    }
}

// ==================== YalnCube ====================

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

    m_vertices = std::move(cubeVertices);
    m_indices  = std::move(cubeIndices);
}

// ==================== YalnPyramid ====================

// 四棱锥：底面矩形（w×l）中心在原点、位于 z=-h/2，顶点在 z=+h/2
YalnPyramid::YalnPyramid(float w, float l, float h) {
    const float hw = w * 0.5f;
    const float hl = l * 0.5f;
    const float hh = h * 0.5f;

    // 顶点：底面四角 + 顶点（颜色：四个侧面暖色渐变，底面土色）
    std::vector<YalnVertexPC> pyramidVertices = {
        {{-hw, -hl, -hh}, {1.00f, 0.55f, 0.25f}},  // 0 南西
        {{ hw, -hl, -hh}, {0.95f, 0.45f, 0.20f}},  // 1 南东
        {{ hw,  hl, -hh}, {0.90f, 0.40f, 0.18f}},  // 2 北东
        {{-hw,  hl, -hh}, {1.00f, 0.50f, 0.22f}},  // 3 北西
        {{  0,   0,  hh}, {1.00f, 0.75f, 0.35f}},  // 4 顶点
    };

    // 索引（外看逆时针）：四个侧面 + 底面两三角
    std::vector<uint32_t> pyramidIndices = {
        // 南面 (y=-hl)
        0, 1, 4,
        // 东面 (x=+hw)
        1, 2, 4,
        // 北面 (y=+hl)
        2, 3, 4,
        // 西面 (x=-hw)
        3, 0, 4,
        // 底面 (z=-hh，法向 -Z)
        0, 2, 1,
        0, 3, 2
    };

    m_vertices = std::move(pyramidVertices);
    m_indices  = std::move(pyramidIndices);
}

// ==================== YalnSphere ====================

// 球体构造函数 (经纬度法)
YalnSphere::YalnSphere(float radius, int segments)
    : m_radius(radius), m_segments(segments) {

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

            m_vertices.push_back({pos, color});
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
            m_indices.push_back(i0);
            m_indices.push_back(i1);
            m_indices.push_back(i2);

            m_indices.push_back(i1);
            m_indices.push_back(i3);
            m_indices.push_back(i2);
        }
    }
}
