#version 450

#define PI 3.14159
#define GAMMA 2.2

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;

layout (location = 0) out vec4 outColor;

layout (binding = 1) uniform LightUniformBufferObject
{
	vec3 position;
	vec3 color;
    float intensify;
    float constant;
    float linear;
    float quadratic;
} lightUBO;

layout (binding = 2) uniform MaterialUniformBufferObject
{
    vec3 baseColor;
    float metallic;
    float roughness;
    float ao;
}  materialUBO;

//----------------------------------------------------------

vec3 Lambertian_Diffuse(vec3 baseColor, vec3 F)
{
    vec3 Ks = F;
    vec3 Kd = vec3(1.0) - Ks;
    return Kd * baseColor / PI;
}

vec3 Diffuse_Disney(vec3 baseColor, float roughness, float dotNL, float dotNV, float dotVH)
{
	float FD90 = 0.5 + 2.0 * roughness * dotVH * dotVH;
	float FL = (1.0 + (FD90 - 1.0) * pow((1.0 - dotNL), 5.0));
	float FV = (1.0 + (FD90 - 1.0) * pow((1.0 - dotNV), 5.0));
	return baseColor / PI * FL * FV;
}

float D_GGX(float roughness, float dotNH)
{
	float FNH = pow(((dotNH * dotNH) * (roughness * roughness - 1.0) + 1.0), 2.0);
	return (roughness * roughness) / (PI * FNH);
}

vec3 F_Schlick(vec3 baseColor, float metallic, float dotVH)
{
    vec3 f0 = mix(vec3(0.04), baseColor, metallic);
	return f0 + (vec3(1.0, 1.0, 1.0) - f0) * pow((1.0 - dotVH), 5.0);
}

float G_SchlickGGX(float roughness, float dotNL, float dotNV)
{
	float alpha = pow((roughness + 1.0) / 2.0, 2.0);
	float k = alpha / 2.0;
	float GL = dotNL / (dotNL * (1.0 - k) + k);
	float GV = dotNV / (dotNV * (1.0 - k) + k);
	return GL * GV;
}

vec3 Specular_CookTorrance(float D, vec3 F, float G, float dotNL, float dotNV)
{
	return (D * F * G) / (4.0 * dotNL * dotNV + 0.001);
}


vec3 BRDF(vec3 N, vec3 L, vec3 V, vec3 baseColor, float metallic, float roughness)
{
	vec3 H = normalize(V + L);
	float dotNL = clamp(dot(N, L), 0.0, 1.0);
	float dotNV = clamp(dot(N, V), 0.0, 1.0);
	float dotVH = clamp(dot(V, H), 0.0, 1.0);
	float dotNH = clamp(dot(N, H), 0.0, 1.0);

    roughness = max(roughness, 0.05);

	//Specular BRDF
	float D = D_GGX(roughness, dotNH);
	vec3 F = F_Schlick(baseColor, metallic, dotVH);
	float G = G_SchlickGGX(roughness, dotNL, dotNV);
	vec3 BRDF_Specular = Specular_CookTorrance(D, F, G, dotNL, dotNV);

    //Diffuse BRDF
	vec3 BRDF_Diffuse = Diffuse_Disney(baseColor, roughness, dotNL, dotNV, dotVH);

	return BRDF_Diffuse + BRDF_Specular;
}

//----------------------------------------------------------

void main() 
{
	vec3 Light = normalize(lightUBO.position - inPosition);
	vec3 View = normalize(vec3(0.f, 0.f, 0.f) - inPosition); //视图空间中摄像机位于原点

	float lightDistance = distance(inPosition, lightUBO.position);
    float attenuationIntensify = lightUBO.intensify / (lightUBO.constant + lightUBO.linear * lightDistance + lightUBO.quadratic * lightDistance * lightDistance);

    vec3 lightColor = lightUBO.color * attenuationIntensify;

	float dotNL = clamp(dot(inNormal, Light), 0.0, 1.0);
    vec3 color = BRDF(inNormal, Light, View, materialUBO.baseColor, materialUBO.metallic, materialUBO.roughness) * dotNL * lightUBO.color;
    color += materialUBO.baseColor * lightUBO.color * 0.01;
    color += vec3(materialUBO.ao);
	color = pow(color, vec3(1.0 / GAMMA));

	outColor = vec4(color, 1.0);
}