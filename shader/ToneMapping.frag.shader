#version 460
#pragma shader_stage(fragment)

layout(set = 0, binding = 0) uniform sampler2D u_HDRScene;
layout(push_constant) uniform OutputPush {
    vec4 data; // x: 0 SDR-manual, 1 SDR-sRGB attachment, 2 scRGB, 3 HDR10-PQ; y: exposure
} outputInfo;
layout(location = 0) in vec2 i_TexCoord;
layout(location = 0) out vec4 o_Color;

vec3 LinearToSrgb(vec3 value) {
    bvec3 low = lessThanEqual(value, vec3(0.0031308));
    vec3 lower = value * 12.92;
    vec3 higher = 1.055 * pow(max(value, vec3(0.0)), vec3(1.0 / 2.4)) - 0.055;
    return mix(higher, lower, low);
}

vec3 AcesApprox(vec3 value) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((value * (a * value + b)) / (value * (c * value + d) + e), 0.0, 1.0);
}

vec3 Rec709ToRec2020(vec3 color) {
    return mat3(
        0.6274, 0.0691, 0.0164,
        0.3293, 0.9195, 0.0880,
        0.0433, 0.0114, 0.8956) * color;
}

vec3 LinearToPQ(vec3 linearNits) {
    const float m1 = 2610.0 / 16384.0;
    const float m2 = 2523.0 / 32.0;
    const float c1 = 3424.0 / 4096.0;
    const float c2 = 2413.0 / 128.0;
    const float c3 = 2392.0 / 128.0;
    vec3 normalized = clamp(linearNits / 10000.0, 0.0, 1.0);
    vec3 p = pow(normalized, vec3(m1));
    return pow((c1 + c2 * p) / (1.0 + c3 * p), vec3(m2));
}

void main() {
    vec3 hdr = texture(u_HDRScene, i_TexCoord).rgb * outputInfo.data.y;
    float mode = outputInfo.data.x;
    if (mode < 1.5) {
        vec3 sdrLinear = AcesApprox(hdr);
        o_Color = vec4(mode > 0.5 ? sdrLinear : LinearToSrgb(sdrLinear), 1.0);
    } else if (mode < 2.5) {
        // scRGB/extended-sRGB-linear keeps linear floating-point values and allows values above 1.
        o_Color = vec4(hdr, 1.0);
    } else {
        // Treat 1.0 scene-linear as 100 nits, convert primaries, then apply ST.2084 PQ.
        o_Color = vec4(LinearToPQ(Rec709ToRec2020(hdr) * 100.0), 1.0);
    }
}
