#version 450

layout (location = 0) in vec3 inTexCoord;

layout (location = 0) out vec4 outColor;

layout (binding = 1) uniform samplerCube texSamplerCubeMap; //注意类型为samplerCube

void main() {
    outColor = texture(texSamplerCubeMap, inTexCoord);
}