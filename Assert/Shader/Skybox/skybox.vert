#version 450

layout (location = 0) in vec3 inPosition;

layout (location = 0) out vec3 outTexCoord;

layout (binding = 0) uniform UniformBufferObject
{
	mat4 model;
	mat4 proj;
} ubo;

void main() {
	outTexCoord = inPosition;
	//outTexCoord.xy *= -1.0; // Convert cubemap coordinates into Vulkan coordinate space

    gl_Position = ubo.proj * ubo.model * vec4(inPosition, 1.0); //no "view" component
}