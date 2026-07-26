#version 460
#pragma shader_stage(fragment)

layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput u_Albedo;
layout(input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput u_Normal;
layout(input_attachment_index = 2, set = 0, binding = 2) uniform subpassInput u_Position;
layout(input_attachment_index = 3, set = 0, binding = 3) uniform subpassInput u_Depth;

layout(push_constant) uniform DeferredDisplayPush {
    vec4 data; // x = 0 lighting, 1 albedo, 2 normal, 3 position
} display;

layout(location = 0) out vec4 o_Color;

void main() {
    vec3 albedo = subpassLoad(u_Albedo).rgb;
    vec3 normal = normalize(subpassLoad(u_Normal).xyz);
    vec3 position = subpassLoad(u_Position).xyz;
    float depth = subpassLoad(u_Depth).r;

    if (display.data.x > 0.5 && display.data.x < 1.5) {
        o_Color = vec4(albedo, 1.0);
        return;
    }
    if (display.data.x > 1.5 && display.data.x < 2.5) {
        o_Color = vec4(normal * 0.5 + 0.5, 1.0);
        return;
    }
    if (display.data.x > 2.5) {
        o_Color = vec4(abs(position) / (abs(position) + vec3(1.0)), 1.0);
        return;
    }

    if (depth >= 1.0) {
        o_Color = vec4(0.015, 0.02, 0.035, 1.0);
        return;
    }
    vec3 lightDirection = normalize(vec3(0.45, 0.8, 0.35));
    float diffuse = max(dot(normal, lightDirection), 0.0);
    vec3 ambient = 0.12 * albedo;
    vec3 litColor = ambient + albedo * diffuse;
    o_Color = vec4(litColor, 1.0);
}
