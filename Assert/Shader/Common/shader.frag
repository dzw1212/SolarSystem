#version 450

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec3 inColor;
layout (location = 3) in vec4 inShadowCoord;
layout (location = 4) in vec3 inLightPos;

layout (location = 0) out vec4 outColor;

layout (binding = 1) uniform sampler2D shadowMapSampler;

float textureProj(vec4 shadowCoord)
{
	float shadow = 1.0;
	if ( shadowCoord.z > -1.0 && shadowCoord.z < 1.0 ) 
	{
		float dist = texture( shadowMapSampler, shadowCoord.st ).r;
		if ( shadowCoord.w > 0.0 && dist < shadowCoord.z ) 
		{
			shadow = 0.1;
		}
	}
	return shadow;
}

void main() 
{
    vec3 lightColor = vec3(1.0, 1.0, 1.0);
    vec3 lightPosition = inLightPos;
    float lightIntensify = 5.0;
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

    // float lightPovDepth = texture(shadowMapSampler, inShadowCoord.xy).r;
    // float IsNotInShadow = (inShadowCoord.z < lightPovDepth) ? 0.0 : 1.0;
    // //float IsNotInShadow = 0.0;

    

    // vec3 shadowCoord = inShadowCoord.xyz / inShadowCoord.w;
    // shadowCoord = shadowCoord * 0.5 + 0.5;
    // float closetDepth = texture(shadowMapSampler, shadowCoord.xy).r;
    // float currentDepth = shadowCoord.z;

    // float NotInShadow = currentDepth < closetDepth ? 1.0 : 0.0;
    float InShadow = textureProj(inShadowCoord / inShadowCoord.w);

	outColor = vec4(ambient + diffuse * InShadow + specular * InShadow, 1.0);
}