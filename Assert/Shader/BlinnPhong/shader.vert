#version 450

layout (location = 0) in vec3 inPosition;
layout (location = 3) in vec3 inNormal;

layout (location = 0) out vec3 outPosition;
layout (location = 1) out vec3 outNormal;

layout (binding = 0) uniform MVPUniformBufferObject
{
	mat4 model;
	mat4 view;
	mat4 proj;
	mat4 mv_normal; //用于将normal转到视图空间
} mvpUBO;

void main()
{
	outPosition = (mvpUBO.view * mvpUBO.model * vec4(inPosition, 1.0)).xyz; //转为视图空间进行运算
	outNormal = normalize((mvpUBO.mv_normal * vec4(inNormal, 1.0)).xyz);

    gl_Position = mvpUBO.proj * mvpUBO.view * mvpUBO.model * vec4(inPosition, 1.0);
}