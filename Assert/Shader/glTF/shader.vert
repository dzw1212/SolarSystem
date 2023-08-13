#version 450

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inTexCoord;
layout (location = 3) in vec2 inNormal;

layout (binding = 0) uniform MVPUniformBufferObject
{
	mat4 model;
	mat4 view;
	mat4 proj;
} MVPubo;

void main() 
{
    gl_Position = MVPubo.proj * MVPubo.view * MVPubo.model * vec4(inPosition, 1.0);
}