#version 460
#pragma shader_stage(fragment)

layout(location = 0) in vec3 i_Albedo;
layout(location = 1) in vec3 i_Normal;
layout(location = 2) in vec3 i_WorldPosition;
layout(location = 0) out vec4 o_Albedo;
layout(location = 1) out vec4 o_Normal;
layout(location = 2) out vec4 o_Position;

void main() {
    o_Albedo = vec4(i_Albedo, 1.0);
    o_Normal = vec4(normalize(i_Normal), 1.0);
    o_Position = vec4(i_WorldPosition, 1.0);
}
