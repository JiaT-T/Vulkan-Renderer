#version 460
#pragma shader_stage(fragment)

layout(location = 0) in vec3 i_Color;
layout(location = 1) in vec3 i_Normal;
layout(location = 0) out vec4 o_Color;

void main() {
    vec3 lightDirection = normalize(vec3(0.45, 0.8, 0.35));
    float diffuse = max(dot(normalize(i_Normal), lightDirection), 0.0);
    o_Color = vec4(i_Color * (0.18 + 0.82 * diffuse), 1.0);
}
