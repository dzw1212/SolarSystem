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
	vec3 position;
	vec4 ambient;
	vec4 diffuse;
	vec4 specular;
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
	vec4 Pos = mvpUBO.view * mvpUBO.model * vec4(inPosition, 1.0); //转为视图空间进行运算
	vec3 Normal = normalize((mvpUBO.mv_normal * vec4(inNormal, 1.0)).xyz);
	vec3 Light = normalize(lightUBO.position - Pos.xyz);
	vec3 View = normalize(vec3(0.f, 0.f, 0.f) - Pos.xyz); //视图空间中摄像机位于原点
	vec3 Half = normalize(View + Light);

	vec4 ambient = lightUBO.ambient * materialUBO.ambient;
	vec4 diffuse = lightUBO.diffuse * materialUBO.diffuse * max(0.0, dot(Normal, Light));
	vec4 specular = lightUBO.specular * materialUBO.specular * pow(max(0.0, dot(Normal, Half)), materialUBO.shininess);

	outColor = ambient + diffuse + specular;

    gl_Position = mvpUBO.proj * mvpUBO.view * mvpUBO.model * vec4(inPosition, 1.0);
}