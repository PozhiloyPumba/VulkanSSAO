#version 450

#include "common.h"

layout (binding = 0) uniform sampler2D samplerposition;
layout (binding = 1) uniform sampler2D samplerNormal;
layout (binding = 2) uniform sampler2D samplerAlbedo;
layout (binding = 5) uniform UBO 
{
	mat4 _dummy;
	int ao;
	int aoOnly;
	int aoBlur;
} uboParams;

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;

void main() 
{
	vec3 fragPos = texture(samplerposition, inUV).rgb;
	vec3 normal = Decode(texture(samplerNormal, inUV).rg);
	vec4 albedo = texture(samplerAlbedo, inUV);

	vec3 lightPos = vec3(0.0);
	vec3 L = normalize(lightPos - fragPos);
	float NdotL = max(0.5, dot(normal, L));

	vec3 baseColor = albedo.rgb * NdotL;

	outFragColor.rgb = baseColor;
}