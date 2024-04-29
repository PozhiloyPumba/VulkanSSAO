#version 450
#extension GL_GOOGLE_include_directive : enable

#include "common_blur.h"

layout (binding = 0) uniform sampler2D samplerAO;
layout (binding = 1) uniform sampler2D depth;

layout (binding = 2) uniform UBO 
{
	int depth_check;
	float depth_range;
	float nearPlane;
	float farPlane;
	int useLerpTrick;
} ubo;

layout (location = 0) in vec2 inUV;

layout (location = 0) out float outFragColor;

float linearDepth(float depth)
{
	float z = depth * 2.0f - 1.0f; 
	return (2.0f * ubo.nearPlane * ubo.farPlane) / (ubo.farPlane + ubo.nearPlane - z * (ubo.farPlane - ubo.nearPlane));	
}

void main() 
{
	vec2 texelSize = 1.0 / vec2(textureSize(samplerAO, 0));
	float result = texture(samplerAO, inUV).r * weight_0;
	if (bool(ubo.useLerpTrick)) {
		for (int i = 1; i < 3; ++i) {
			vec2 uvOffset = vec2(offset_lerp[i], 0.f) * texelSize;
			result += texture(samplerAO, inUV + uvOffset).r * weight_lerp[i];
			result += texture(samplerAO, inUV - uvOffset).r * weight_lerp[i];
		}
	} else {
		const ivec2 offsets[4] = ivec2[](ivec2(1, 0), 
										 ivec2(2, 0), 
										 ivec2(3, 0), 
										 ivec2(4, 0));
		vec4 aoComp = textureGatherOffsets(samplerAO, inUV, offsets, 0);
		result += dot(aoComp, weights);

		const ivec2 offsets_neg[4] = ivec2[](ivec2(-1, 0), 
											 ivec2(-2, 0), 
											 ivec2(-3, 0), 
											 ivec2(-4, 0));
		aoComp = textureGatherOffsets(samplerAO, inUV, offsets_neg, 0);
		result += dot(aoComp, weights);
	}
	outFragColor = result;
}