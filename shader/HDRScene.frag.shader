#version 460
#pragma shader_stage(fragment)

layout(push_constant) uniform HDRScenePush {
    vec4 data; // x = time
} scene;
layout(location = 0) in vec2 i_TexCoord;
layout(location = 0) out vec4 o_Color;

void main() {
    vec2 uv = i_TexCoord;
    vec3 gradient = mix(vec3(0.03, 0.05, 0.12), vec3(1.4, 0.35, 0.08), uv.x);
    vec2 center = vec2(0.72 + 0.08 * sin(scene.data.x * 0.6), 0.42);
    float highlight = exp(-70.0 * dot(uv - center, uv - center));
    vec3 hdrColor = gradient + highlight * vec3(8.0, 5.0, 2.0);
    o_Color = vec4(hdrColor, 1.0);
}
