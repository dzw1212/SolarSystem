#version 450

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inTexCoord;

layout (location = 0) out vec3 fragColor;
layout (location = 1) out vec2 fragTexCoord;
layout (location = 2) out float textureLod;

layout (binding = 0) uniform UniformBufferObject
{
	mat4 view;
	mat4 proj;
	float lod;
} ubo;

layout (binding = 1) uniform DynamicUniformBufferObject
{
	mat4 model;
} dynamicUbo;

layout (binding = 2) uniform sampler2DArray texSampler;

void main() {
    gl_Position = ubo.proj * ubo.view * dynamicUbo.model * vec4(inPosition, 1.0);
    fragColor = inColor;
	fragTexCoord = inTexCoord;
	textureLod = ubo.lod;
}