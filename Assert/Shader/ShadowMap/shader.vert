#version 450

layout (location = 0) in vec3 inPosition;

layout (binding = 0) uniform MVPUniformBufferObject
{
    mat4 mvp;
} mvpUBO;

void main() 
{
    gl_Position = mvpUBO.mvp * vec4(inPosition, 1.0);
}