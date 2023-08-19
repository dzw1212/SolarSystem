#version 450

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec3 inColor;

layout (location = 0) out vec4 outColor;

void main() 
{
    vec3 lightColor = vec3(1.0, 1.0, 1.0);
    vec3 lightPosition = vec3(0.0, 0.0, 0.0); //与摄像机一致
    float lightIntensify = 1.0;
    float constant = 10.0;
    float linear = 0.09;
    float quadratic = 0.032; //GPT告诉我的常用的衰减系数

    float lightDistance = distance(inPosition, lightPosition);
    float attenuation = lightIntensify / (constant + linear * lightDistance + quadratic * lightDistance * lightDistance);
    lightColor *= attenuation;

    vec3 materialAmbient = inColor;
    vec3 materialDiffuse = inColor;
    vec3 materialSpecular = vec3(1.0, 1.0, 1.0);

	vec3 Light = normalize(lightPosition - inPosition);
	vec3 View = normalize(vec3(0.f, 0.f, 0.f) - inPosition); //视图空间中摄像机位于原点
	vec3 Half = normalize(View + Light);

	vec3 ambient = lightColor * materialAmbient;
	vec3 diffuse = lightColor * materialDiffuse * max(0.0, dot(inNormal, Light));
	vec3 specular = lightColor * materialSpecular * pow(max(0.0, dot(inNormal, Half)), 1.0);

	outColor = vec4(ambient + diffuse + specular, 1.0);
}