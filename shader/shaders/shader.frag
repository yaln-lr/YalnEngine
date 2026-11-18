#version 450

// 从顶点着色器接收颜色与世界坐标
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 worldPos;

// 输出颜色到帧缓冲区
layout(location = 0) out vec4 outColor;

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

// ============================================================================
//  Blinn-Phong 光照
//  法线：屏幕空间导数求面法线（平面着色）——网格无需携带法线属性，
//        三角形内部法线恒定，天然契合低多边形美术风格
// ============================================================================
void main() {
    // dFdx/dFdy 叉积得到当前三角形的世界空间法线
    vec3 N = normalize(cross(dFdx(worldPos), dFdy(worldPos)));
    // 视线方向（片段 → 相机）
    vec3 V = normalize(ubo.camera_pos.xyz - worldPos);

    // 环境光底色
    vec3 result = fragColor * ubo.ambient.rgb;

    for (int i = 0; i < ubo.counts.x && i < 8; ++i) {
        Light L = ubo.lights[i];
        int   type      = int(L.params.w + 0.5);
        float intensity = L.position.w;
        vec3  Ldir;
        float atten = 1.0;
        float spotF = 1.0;

        if (type == 1) {
            // 平行光：position.xyz = 光线传播方向，无衰减
            Ldir = normalize(-L.position.xyz);
        } else {
            // 点光 / 聚光：position.xyz = 光源世界坐标
            vec3 toLight = L.position.xyz - worldPos;
            float d = length(toLight);
            if (d < 1e-4 || d > L.params.x) {
                continue;   // 超出作用半径
            }
            Ldir = toLight / d;
            // 平滑距离衰减：(1 - d/range)^2
            float f = clamp(1.0 - d / L.params.x, 0.0, 1.0);
            atten = f * f;

            if (type == 3) {
                // 聚光：光轴方向与"光源→片段"方向的夹角余弦
                float cosA = dot(normalize(L.position.xyz), -Ldir);
                // 内锥全亮 → 外锥线性过渡 → 外锥外熄灭（平方软化边缘）
                float t = clamp((cosA - L.params.z) / max(L.params.y - L.params.z, 1e-4), 0.0, 1.0);
                spotF = t * t;
                if (spotF <= 0.0) {
                    continue;
                }
            }
        }

        // Blinn-Phong：漫反射 + 半程向量高光
        float diff = max(dot(N, Ldir), 0.0);
        vec3  H    = normalize(Ldir + V);
        float spec = pow(max(dot(N, H), 0.0), 32.0);

        // 高光用固定中性色（约半强度白色），避免把顶点色过度提亮
        result += L.color.rgb * intensity * atten * spotF *
                  (fragColor * diff + vec3(0.30, 0.29, 0.27) * spec);
    }

    outColor = vec4(result, 1.0);
}
