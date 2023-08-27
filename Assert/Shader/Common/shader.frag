#version 450

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec3 inColor;
layout (location = 3) in vec4 inShadowCoord;
layout (location = 4) in vec3 inLightPos;

layout (location = 0) out vec4 outColor;

layout (binding = 1) uniform sampler2D shadowMapSampler;

float PCF(vec4 shadowCoord, float filterSize)
{
	filterSize = (filterSize > 0.0) ? filterSize : -1.f * filterSize; //非负数
	filterSize = round(filterSize); //非小数
	filterSize = (mod(filterSize, 2.0) == 0.0) ? filterSize + 1.0 : filterSize; //非偶数
	filterSize = (filterSize < 3.0) ? 3.0 : filterSize; //最小3.0
	//filterSize = (filterSize > 21.0) ? 21.0 : filterSize; //最大9.0

	vec2 shadowMapUV = shadowCoord.xy / shadowCoord.w;
	vec2 texelSize = 1.0 / textureSize(shadowMapSampler, 0);
	float currentDepth = shadowCoord.z / shadowCoord.w;
	float closetDepth = 0.0;

	float filterHalf = floor(filterSize / 2.0);
	float shadowCount = 0.0;
	float shadowTotal = 0.0;
	for (float i = -filterHalf; i <= filterHalf; ++i)
	{
		for (float j = -filterHalf; j <= filterHalf; ++j)
		{
			closetDepth = texture(shadowMapSampler, shadowMapUV + vec2(i, j) * texelSize).r;
			shadowTotal += (currentDepth > closetDepth) ? 1.0 : 0.0;
			shadowCount++;
		}
	}

	return shadowTotal / shadowCount;
}

float PCSS(vec4 shadowCoord)
{
	vec2 shadowMapUV = shadowCoord.xy / shadowCoord.w;
	vec2 texelSize = 1.0 / textureSize(shadowMapSampler, 0);
	float receiveDepth = shadowCoord.z / shadowCoord.w;

	//Blocker Search
	float searchSize = 5.0;
	float searchHalf = floor(searchSize / 2.0);
	float searchArea = searchSize * searchSize;
	float blockerDepth = 0.0;
	float blockerDepthTotal = 0.0;
	float blockerAverage = 0.0;
	for (float i = -searchHalf; i <= searchHalf; ++i)
	{
		for (float j = -searchHalf; j <= searchHalf; ++j)
		{
			blockerDepth = texture(shadowMapSampler, shadowMapUV + vec2(i, j) * texelSize).r;
			blockerDepthTotal += (blockerDepth < receiveDepth) ? blockerDepth : 0.0;
		}
	}
	blockerAverage = blockerDepthTotal / searchArea;

	if (blockerAverage == 0.0)
		return 0.0;

	//Penumbra Estimation
	//假设点光源的大小为1
	float PenumbraWidth = (receiveDepth - blockerAverage) / blockerAverage;

	return (PenumbraWidth < 0.0) ? 0.0 : PCF(shadowCoord, PenumbraWidth / 3.0);
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

    // float closetDepth = texture(shadowMapSampler, inShadowCoord.xy / inShadowCoord.w).r;
    // float IsInShadow = closetDepth < (inShadowCoord.z / inShadowCoord.w) ? 1.0 : 0.0;
    // float IsNotInShadow = 1.0 - IsInShadow;

	//float IsNotInShadow = 1.0 - PCF(inShadowCoord, 7.0);

	float IsNotInShadow = 1.0 - PCSS(inShadowCoord);

	outColor = vec4(ambient + diffuse * IsNotInShadow + specular * IsNotInShadow, 1.0);
}