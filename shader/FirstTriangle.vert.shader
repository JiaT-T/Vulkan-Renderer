#version 460
#pragma shader_stage(vertex)

layout(push_constant) uniform PushConstants {
    vec4 color;
    vec2 scale;
} pushConstants;

layout(location = 0) in vec2 i_Position;
layout(location = 1) in vec4 i_Color;
layout(location = 2) in vec2 i_InstanceOffset;
layout(location = 3) in vec4 i_InstanceColor;
layout(location = 0) out vec4 o_Color;

void main() {
    gl_Position = vec4(i_Position * pushConstants.scale + i_InstanceOffset, 0.0, 1.0);
    o_Color = i_Color * i_InstanceColor * pushConstants.color;
}
