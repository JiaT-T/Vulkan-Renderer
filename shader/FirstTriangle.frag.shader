#version 460
#pragma shader_stage(fragment)

layout(location = 0) in vec4 i_Color;
layout(location = 1) in vec2 i_TexCoord;
layout(location = 0) out vec4 o_Color;
layout(set = 0, binding = 1) uniform sampler2D u_Texture;

void main() {
    o_Color = texture(u_Texture, i_TexCoord) * i_Color;
}
