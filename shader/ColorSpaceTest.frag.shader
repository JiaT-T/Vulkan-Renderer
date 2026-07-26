#version 460
#pragma shader_stage(fragment)

layout(push_constant) uniform ColorSpacePush {
    vec4 data; // x = 1 when the swapchain attachment performs sRGB encoding
} outputInfo;
layout(location = 0) in vec2 i_TexCoord;
layout(location = 0) out vec4 o_Color;

float SrgbToLinear(float value) {
    return value <= 0.04045 ? value / 12.92 : pow((value + 0.055) / 1.055, 2.4);
}

float LinearToSrgb(float value) {
    return value <= 0.0031308 ? value * 12.92 : 1.055 * pow(value, 1.0 / 2.4) - 0.055;
}

void main() {
    // 50% in encoded/gamma space is too dark for the average of black and white.
    float wrongEncoded = 0.5;
    float correctLinear = 0.5;
    float linearForAttachment = i_TexCoord.x < 0.5 ? SrgbToLinear(wrongEncoded) : correctLinear;
    float outputValue = outputInfo.data.x > 0.5
        ? linearForAttachment
        : LinearToSrgb(linearForAttachment);

    // Thin black/white strips retain reference endpoints around the comparison.
    if (i_TexCoord.y < 0.08)
        outputValue = step(0.5, fract(i_TexCoord.x * 16.0));
    o_Color = vec4(vec3(outputValue), 1.0);
}
