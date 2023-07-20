#version 450 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;

layout(push_constant) uniform uPushConstant {
    vec2 uScale;
    vec2 uTranslate;
} pc;

out gl_PerVertex {
    vec4 gl_Position;
};

layout(location = 0) out struct {
    vec4 Color;
    vec2 UV;
} Out;

vec4 GammaCorrection(vec4 color)
{
    float gamma = 2.2;
    return vec4(pow(color.x, gamma), pow(color.y, gamma), pow(color.z, gamma), color.w);
}

void main()
{
    Out.Color = GammaCorrection(aColor);
    Out.UV = aUV;
    gl_Position = vec4(aPos * pc.uScale + pc.uTranslate, 0, 1);
}
