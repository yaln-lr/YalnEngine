#version 450

// 白膜专用片元着色器：忽略顶点颜色，使用统一的中性白反照率。
// 保留与 shader.vert 完全一致的接口（fragColor / worldPos / UBO / PushConstants）
// 和 Blinn-Phong 光照，使白膜仍呈现真实的光照明暗与体积感。

// 从顶点着色器接收（fragColor 在本 shader 中不使用，仅为保持接口一致）
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 worldPos;

layout(location = 0) out vec4 outColor;

struct Light {
    vec4 position;
    vec4 direction;
    vec4 color;
    vec4 params;
};

layout(binding = 0) uniform UniformBufferObject {
    mat4 proj_view_mat;
    vec4 camera_pos;
    vec4 ambient;
    ivec4 counts;
    Light lights[8];
} ubo;

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 base_color;
} pc;

void main() {
    // 白膜反照率：中性亮灰白，保持光照下的明暗层次（纯 1.0 高光会过曝）
    vec3 albedo = vec3(0.85, 0.85, 0.85);

    vec3 N = normalize(cross(dFdx(worldPos), dFdy(worldPos)));
    vec3 V = normalize(ubo.camera_pos.xyz - worldPos);

    vec3 result = albedo * ubo.ambient.rgb;

    for (int i = 0; i < ubo.counts.x && i < 8; ++i) {
        Light L = ubo.lights[i];
        int   type      = int(L.params.w + 0.5);
        float intensity = L.position.w;
        vec3  Ldir;
        float atten = 1.0;
        float spotF = 1.0;

        if (type == 1) {
            Ldir = normalize(-L.direction.xyz);
        } else {
            vec3 toLight = L.position.xyz - worldPos;
            float d = length(toLight);
            if (d < 1e-4 || d > L.params.x) {
                continue;
            }
            Ldir = toLight / d;
            float f = clamp(1.0 - d / L.params.x, 0.0, 1.0);
            atten = f * f;

            if (type == 3) {
                float cosA = dot(normalize(L.direction.xyz), -Ldir);
                float t = clamp((cosA - L.params.z) / max(L.params.y - L.params.z, 1e-4), 0.0, 1.0);
                spotF = t * t;
                if (spotF <= 0.0) {
                    continue;
                }
            }
        }

        float diff = max(dot(N, Ldir), 0.0);
        vec3  H    = normalize(Ldir + V);
        float spec = pow(max(dot(N, H), 0.0), 32.0);

        result += L.color.rgb * intensity * atten * spotF *
                  (albedo * diff + vec3(0.30, 0.29, 0.27) * spec);
    }

    outColor = vec4(result, 1.0);
}
