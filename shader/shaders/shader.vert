#version 450

// 顶点位置
layout(location = 0) in vec3 inPosition;
// 顶点颜色
layout(location = 1) in vec3 inColor;

// 输出颜色到片段着色器
layout(location = 0) out vec3 fragColor;
// 输出世界坐标（片段着色器计算法线/光照方向用）
layout(location = 1) out vec3 worldPos;

// 光源的 GPU 表示（必须与 core/include/yalnlight.h 的 YalnGPULight 一致）
struct Light {
    vec4 position;   // 平行光: xyz=方向 w=强度；点光/聚光: xyz=位置 w=强度
    vec4 color;      // rgb=光色
    vec4 params;     // x=range, y=内锥余弦, z=外锥余弦, w=类型(1/2/3)
};

// UBO - 与 YalnFrameUniform（std140）一一对应
layout(binding = 0) uniform UniformBufferObject {
    mat4 proj_view_mat;
    vec4 camera_pos;      // xyz = 相机世界坐标
    vec4 ambient;         // rgb = 环境光色 × 强度
    ivec4 counts;         // x = 动态光源数量
    Light lights[8];
} ubo;

// Push Constants - 模型矩阵
layout(push_constant) uniform ModelMatrix {
    mat4 model;
} pc;

void main() {
    vec4 world = pc.model * vec4(inPosition, 1.0);
    gl_Position = ubo.proj_view_mat * world;
    fragColor = inColor;
    worldPos = world.xyz;
}
