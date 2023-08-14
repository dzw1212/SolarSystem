#version 450

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inTexCoord;
layout (location = 3) in vec2 inNormal;

layout (location = 0) out vec2 outTexCoord;

layout (push_constant) uniform MVPPushConstant
{
    mat4 model;
    mat4 view;
    mat4 proj;
} MVPpc;

void main() 
{
    gl_Position = MVPpc.proj * MVPpc.view * MVPpc.model * vec4(inPosition, 1.0);
}