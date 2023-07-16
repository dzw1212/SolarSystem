#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in float textureLod;
layout(location = 3) in vec3 inUV;

layout(location = 0) out vec4 outColor;

layout (binding = 0) uniform UniformBufferObject
{
	mat4 view;
	mat4 proj;
	float lod;
} ubo;

layout (binding = 1) uniform DynamicUniformBufferObject
{
	mat4 model;
	float textureIndex;
} dynamicUbo;

layout (binding = 2) uniform sampler2DArray texSampler;

void main() {
    outColor = texture(texSampler, inUV, textureLod);
    //outColor = vec4(fragColor,1.0);
}