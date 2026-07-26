#version 460
#pragma shader_stage(fragment)

layout(set = 0, binding = 0) uniform sampler2D u_Depth;
layout(push_constant) uniform DepthDisplayPush {
    vec4 params; // x = near, y = far, z = 0 raw / 1 linear
} display;
layout(location = 0) in vec2 i_TexCoord;
layout(location = 0) out vec4 o_Color;

float LinearizeDepth(float depth, float nearPlane, float farPlane) {
    // Vulkan perspective depth is in [0, 1] and is not world-space distance.
    return nearPlane * farPlane / (farPlane - depth * (farPlane - nearPlane));
}

void main() {
    float rawDepth = texture(u_Depth, i_TexCoord).r;
    float value = rawDepth;
    if (display.params.z > 0.5) {
        float linearDepth = LinearizeDepth(rawDepth, display.params.x, display.params.y);
        value = clamp((linearDepth - display.params.x) /
            (display.params.y - display.params.x), 0.0, 1.0);
        // Expand the dark near range so the visualization remains useful.
        value = pow(value, 0.35);
    }
    o_Color = vec4(vec3(value), 1.0);
}
