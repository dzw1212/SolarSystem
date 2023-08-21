#version 450

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inTexCoord;
layout (location = 3) in vec3 inNormal;

layout (location = 0) out vec3 outPosition;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec3 outColor;
layout (location = 3) out vec4 outShadowCoord;
layout (location = 4) out vec3 outLightPos;

layout (binding = 0) uniform MVPUniformBufferObject
{
	mat4 model;
	mat4 view;
	mat4 proj;
	mat4 mv_normal; //用于将normal转到视图空间
	mat4 lightPovMVP;
	mat4 bias;
	vec3 lightPos;
} mvpUBO;

void main()
{
    outPosition = (mvpUBO.view * mvpUBO.model * vec4(inPosition, 1.0)).xyz; //转为视图空间进行运算
	outNormal = normalize((mvpUBO.mv_normal * vec4(inNormal, 1.0)).xyz);
	outColor = inColor;
	outLightPos = (mvpUBO.view * mvpUBO.model * vec4(mvpUBO.lightPos, 1.0)).xyz;

	// vec4 clipPos = mvpUBO.lightPovMVP * vec4(inPosition, 1.0);
	// vec4 ndcPos = clipPos / clipPos.w;
	// outShadowCoord = mvpUBO.bias * ndcPos;

    outShadowCoord = mvpUBO.lightPovMVP * vec4(inPosition, 1.0);

    gl_Position = mvpUBO.proj * mvpUBO.view * mvpUBO.model * vec4(inPosition, 1.0);
}
