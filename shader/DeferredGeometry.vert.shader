#version 460
#pragma shader_stage(vertex)

layout(set = 0, binding = 0, std140) uniform DeferredSceneUniform {
    mat4 view;
    mat4 projection;
    mat4 models[3];
    vec4 nearFarMode;
} scene;

layout(location = 0) in vec3 i_Position;
layout(location = 1) in vec3 i_Normal;
layout(location = 2) in vec3 i_Color;
layout(location = 0) out vec3 o_Albedo;
layout(location = 1) out vec3 o_Normal;
layout(location = 2) out vec3 o_WorldPosition;

void main() {
    mat4 model = scene.models[gl_InstanceIndex];
    vec4 worldPosition = model * vec4(i_Position, 1.0);
    gl_Position = scene.projection * scene.view * worldPosition;
    o_Albedo = i_Color;
    // The inverse-transpose normal matrix remains correct under non-uniform scale.
    o_Normal = normalize(mat3(transpose(inverse(model))) * i_Normal);
    o_WorldPosition = worldPosition.xyz;
}
