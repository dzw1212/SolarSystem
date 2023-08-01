#version 450

layout (location = 0) in vec3 inPosition;
layout (location = 3) in vec3 inNormal;

layout (location = 0) out vec4 outColor;

layout (binding = 0) uniform MVPUniformBufferObject
{
	mat4 model;
	mat4 view;
	mat4 proj;
	mat4 mv_normal; //用于将normal转到视图空间
} mvpUBO;

layout (binding = 1) uniform LightUniformBufferObject
{
    float intensify;
    float constant;
    float linear;
    float quadratic;
	vec3 position;
	vec4 ambient;
	vec4 diffuse;
	vec4 specular;
} lightUBO;

layout (binding = 2) uniform MaterialUniformBufferObject
{
	float shininess;
	vec4 ambient;
	vec4 diffuse;
	vec4 specular;
}  materialUBO;

void main()
{
	vec4 Pos = mvpUBO.view * mvpUBO.model * vec4(inPosition, 1.0); //转为视图空间进行运算
	vec3 Normal = normalize((mvpUBO.mv_normal * vec4(inNormal, 1.0)).xyz);
    vec4 LightPos = mvpUBO.view * vec4(lightUBO.position, 1.0);
	vec3 Light = normalize(LightPos.xyz - Pos.xyz);
	vec3 View = normalize(vec3(0.f, 0.f, 0.f) - Pos.xyz); //视图空间中摄像机位于原点
	vec3 Half = normalize(View + Light);

    float lightDistance = distance(Pos, LightPos);
    float attenuationIntensify = lightUBO.intensify / (lightUBO.constant + lightUBO.linear * lightDistance + lightUBO.quadratic * lightDistance * lightDistance);

	vec4 ambient = lightUBO.ambient * attenuationIntensify * materialUBO.ambient;
	vec4 diffuse = lightUBO.diffuse * attenuationIntensify * materialUBO.diffuse * max(0.0, dot(Normal, Light));
	vec4 specular = lightUBO.specular * attenuationIntensify * materialUBO.specular * pow(max(0.0, dot(Normal, Half)), materialUBO.shininess);

	outColor = ambient + diffuse + specular;

    gl_Position = mvpUBO.proj * mvpUBO.view * mvpUBO.model * vec4(inPosition, 1.0);
}