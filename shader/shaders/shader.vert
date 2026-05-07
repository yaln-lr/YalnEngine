#version 450

// 顶点位置
layout(location = 0) in vec3 inPosition;
// 顶点颜色
layout(location = 1) in vec3 inColor;

// 输出颜色到片段着色器
layout(location = 0) out vec3 fragColor;

// 统一缓冲对象（UBO）
layout(binding = 0) uniform UniformBufferObject {
    mat4 model;      // 模型矩阵（旋转、平移、缩放）
    // mat4 view;       // 视图矩阵（相机位置）
    // mat4 proj;       // 投影矩阵（透视投影）
    mat4 proj_view_mat; // 变换矩阵proj*view
} ubo;

void main() {
    // 应用模型、视图和投影矩阵变换
    // gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    gl_Position = ubo.proj_view_mat * ubo.model * vec4(inPosition, 1.0);
    // 将颜色传递给片段着色器
    fragColor = inColor;
}
