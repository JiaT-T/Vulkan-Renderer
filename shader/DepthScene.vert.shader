#version 460
#pragma shader_stage(vertex)

layout(set = 0, binding = 0, std140) uniform DepthSceneUniform {
    mat4 view;
    mat4 projection;
    mat4 models[3];
    vec4 nearFarMode;
} scene;

layout(location = 0) in vec3 i_Position;
layout(location = 1) in vec3 i_Normal;
layout(location = 2) in vec3 i_Color;
layout(location = 0) out vec3 o_Color;
layout(location = 1) out vec3 o_Normal;

void main() {
    mat4 model = scene.models[gl_InstanceIndex];
    gl_Position = scene.projection * scene.view * model * vec4(i_Position, 1.0);
    o_Color = i_Color;
    o_Normal = normalize(mat3(transpose(inverse(model))) * i_Normal);
}
