#version 460
#pragma shader_stage(fragment)

layout(set = 0, binding = 0) uniform sampler2D u_OffscreenColor;
layout(location = 0) in vec2 i_TexCoord;
layout(location = 0) out vec4 o_Color;

void main() {
    o_Color = texture(u_OffscreenColor, i_TexCoord);
}
