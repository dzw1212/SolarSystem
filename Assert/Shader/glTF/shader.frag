#version 450

layout (location = 0) in vec2 inTexCoord;

layout (location = 0) out vec4 outColor;

layout (binding = 0) uniform sampler2D baseColorSampler;
layout (binding = 1) uniform sampler2D normalSampler;
layout (binding = 2) uniform sampler2D occlusionMetallicRoughnessSampler;

void main() 
{
    outColor = texture(baseColorSampler, inTexCoord);
    //outColor = vec4(1.0, 0.0, 0.0, 0.0);
}