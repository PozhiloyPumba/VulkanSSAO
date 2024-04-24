#version 450

#include "common.h"

layout (binding = 0) uniform sampler2D samplerDepth;
layout (binding = 1) uniform sampler2D samplerNormal;
layout (binding = 2) uniform sampler2D samplerAlbedo;
layout (binding = 3) uniform sampler2D samplerAO;
layout (binding = 4) uniform sampler2D samplerAOBlur;
layout (binding = 5) uniform UBO 
{
	mat4 invProjection;
	int ao;
	int aoOnly;
	int aoBlur;
} uboParams;

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;

vec3 ViewPosFromDepth(float depth) {
    float z = depth * 2.0 - 1.0;

    vec4 clipSpacePosition = vec4(inUV * 2.0 - 1.0, z, 1.0);
    vec4 viewSpacePosition = uboParams.invProjection * clipSpacePosition;

    // Perspective division
    viewSpacePosition /= viewSpacePosition.w;

    return viewSpacePosition.xyz;
}

void main() 
{
	vec3 fragPos = ViewPosFromDepth(texture(samplerDepth, inUV).r);
	vec3 normal = Decode(texture(samplerNormal, inUV).rg);
	vec4 albedo = texture(samplerAlbedo, inUV);
	 
	float ao = (uboParams.aoBlur == 1) ? texture(samplerAOBlur, inUV).r : texture(samplerAO, inUV).r;

	vec3 lightPos = vec3(0.0);
	vec3 L = normalize(lightPos - fragPos);
	float NdotL = max(0.5, dot(normal, L));

	if (uboParams.aoOnly == 1)
	{
		outFragColor.rgb = ao.rrr;
	}
	else
	{
		vec3 baseColor = albedo.rgb * NdotL;

		if (uboParams.ao == 1)
		{
			outFragColor.rgb = ao.rrr;

			if (uboParams.aoOnly != 1)
				outFragColor.rgb *= baseColor;
		}
		else
		{
			outFragColor.rgb = baseColor;
		}
	}
}