#version 460
#pragma shader_stage(fragment)

layout(set = 0, binding = 0) uniform sampler2D u_AlphaTexture;
layout(location = 0) in vec2 i_TexCoord;
layout(location = 0) out vec4 o_Color;

void main() {
    // The selected texture already has the alpha representation expected by its pipeline.
    // In particular, the premultiplied texture must not multiply RGB by alpha again here.
    o_Color = texture(u_AlphaTexture, i_TexCoord);
}
