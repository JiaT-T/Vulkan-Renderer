#version 460
#pragma shader_stage(vertex)

layout(push_constant) uniform ScenePush {
    vec4 data; // x = time, yzw reserved for later chapter examples
} scene;

layout(location = 0) out vec3 o_Color;

void main() {
    const vec2 positions[3] = vec2[](
        vec2( 0.0, -0.72),
        vec2( 0.72, 0.62),
        vec2(-0.72, 0.62)
    );
    const vec3 colors[3] = vec3[](
        vec3(1.0, 0.18, 0.12),
        vec3(0.12, 1.0, 0.24),
        vec3(0.16, 0.35, 1.0)
    );
    float angle = scene.data.x * 0.35;
    mat2 rotation = mat2(cos(angle), -sin(angle), sin(angle), cos(angle));
    gl_Position = vec4(rotation * positions[gl_VertexIndex], 0.0, 1.0);
    o_Color = colors[gl_VertexIndex];
}
