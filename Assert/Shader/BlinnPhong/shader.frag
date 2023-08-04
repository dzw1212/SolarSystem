#version 450

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;

layout (location = 0) out vec4 outColor;

layout (binding = 1) uniform LightUniformBufferObject
{
	vec3 position;
	vec4 ambient;
	vec4 diffuse;
	vec4 specular;
    float intensify;
    float constant;
    float linear;
    float quadratic;
} lightUBO;

layout (binding = 2) uniform MaterialUniformBufferObject
{
	vec4 ambient;
	vec4 diffuse;
	vec4 specular;
	float shininess;
}  materialUBO;

void main() 
{
	vec3 Light = normalize(lightUBO.position - inPosition);
	vec3 View = normalize(vec3(0.f, 0.f, 0.f) - inPosition); //视图空间中摄像机位于原点
	vec3 Half = normalize(View + Light);

    float lightDistance = distance(inPosition, lightUBO.position);
    float attenuationIntensify = lightUBO.intensify / (lightUBO.constant + lightUBO.linear * lightDistance + lightUBO.quadratic * lightDistance * lightDistance);

	vec4 ambient = lightUBO.ambient * attenuationIntensify * materialUBO.ambient;
	vec4 diffuse = lightUBO.diffuse * attenuationIntensify * materialUBO.diffuse * max(0.0, dot(inNormal, Light));
	vec4 specular = lightUBO.specular * attenuationIntensify * materialUBO.specular * pow(max(0.0, dot(inNormal, Half)), materialUBO.shininess);

	outColor = ambient + diffuse + specular;
}