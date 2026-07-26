#version 460
#pragma shader_stage(vertex)

layout(push_constant) uniform AlphaPanelPush {
    vec4 data; // x = panel index 0..2
} panel;

layout(location = 0) out vec2 o_TexCoord;

void main() {
    const vec2 localPositions[6] = vec2[](
        vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(0.0, 1.0),
        vec2(1.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0)
    );
    const vec2 texCoords[6] = vec2[](
        vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(0.0, 1.0),
        vec2(1.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0)
    );
    float panelWidth = 0.58;
    float gap = 0.055;
    float left = -0.94 + panel.data.x * (panelWidth + gap);
    vec2 p = localPositions[gl_VertexIndex];
    gl_Position = vec4(left + p.x * panelWidth, -0.74 + p.y * 1.48, 0.0, 1.0);
    o_TexCoord = texCoords[gl_VertexIndex];
}
