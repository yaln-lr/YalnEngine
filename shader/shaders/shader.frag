#version 450

// 从顶点着色器接收颜色
layout(location = 0) in vec3 fragColor;

// 输出颜色到帧缓冲区
layout(location = 0) out vec4 outColor;

void main() {
    // 将颜色输出到帧缓冲区（alpha通道设为1.0）
    outColor = vec4(fragColor, 1.0);
}
