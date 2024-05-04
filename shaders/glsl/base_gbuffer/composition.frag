#version 450
#extension GL_GOOGLE_include_directive : enable

#include "common.h"

layout (binding = 0) uniform sampler2D samplerDepth;
layout (binding = 1) uniform sampler2D samplerNormal;
layout (binding = 2) uniform sampler2D samplerAlbedo;
layout (binding = 3) uniform UBO 
{
	mat4 projection;
} ubo;

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;

vec3 ViewPosFromDepth(float depth) {
    float z = depth * 2.0 - 1.0;

    vec4 clipSpacePosition = vec4(inUV * 2.0 - 1.0, z, 1.0);
    vec4 viewSpacePosition = inverse(ubo.projection) * clipSpacePosition;

    // Perspective division
    viewSpacePosition /= viewSpacePosition.w;

    return viewSpacePosition.xyz;
}

void main() 
{
	vec3 fragPos = ViewPosFromDepth(texture(samplerDepth, inUV).r);
	vec3 normal = Decode(texture(samplerNormal, inUV).rg);
	vec4 albedo = texture(samplerAlbedo, inUV);

	vec3 lightPos = vec3(0.0);
	vec3 L = normalize(lightPos - fragPos);
	float NdotL = max(0.5, dot(normal, L));

	vec3 baseColor = albedo.rgb * NdotL;

	outFragColor.rgb = baseColor;
}