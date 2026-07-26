#version 460
#pragma shader_stage(vertex)

layout(push_constant) uniform PushConstants {
    vec4 color;
    vec2 scale;
} pushConstants;

layout(set = 0, binding = 0, std140) uniform SceneUniform {
    mat4 model;
    mat4 view;
    mat4 projection;
} scene;

layout(location = 0) in vec2 i_Position;
layout(location = 1) in vec4 i_Color;
layout(location = 2) in vec2 i_TexCoord;
layout(location = 3) in vec2 i_InstanceOffset;
layout(location = 4) in vec4 i_InstanceColor;
layout(location = 0) out vec4 o_Color;
layout(location = 1) out vec2 o_TexCoord;

void main() {
    vec4 localPosition = vec4(i_Position * pushConstants.scale + i_InstanceOffset, 0.0, 1.0);
    gl_Position = scene.projection * scene.view * scene.model * localPosition;
    o_Color = i_Color * mix(vec4(1.0), i_InstanceColor, 0.25) * pushConstants.color;
    o_TexCoord = i_TexCoord;
}
