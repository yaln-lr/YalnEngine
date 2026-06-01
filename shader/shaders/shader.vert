#version 450

// 顶点位置
layout(location = 0) in vec3 inPosition;
// 顶点颜色
layout(location = 1) in vec3 inColor;

// 输出颜色到片段着色器
layout(location = 0) out vec3 fragColor;

// UBO - 视图投影矩阵
layout(binding = 0) uniform UniformBufferObject {
    mat4 proj_view_mat;
} ubo;

// Push Constants - 模型矩阵
layout(push_constant) uniform ModelMatrix {
    mat4 model;
} pc;

void main() {
    gl_Position = ubo.proj_view_mat * pc.model * vec4(inPosition, 1.0);
    fragColor = inColor;
}
