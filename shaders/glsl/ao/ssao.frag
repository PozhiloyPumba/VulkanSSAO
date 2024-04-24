#version 450

#include "common.h"

layout (binding = 0) uniform sampler2D samplerDepth;
layout (binding = 1) uniform sampler2D samplerNormal;
layout (binding = 2) uniform sampler2D ssaoNoise;

layout (constant_id = 0) const int SSAO_KERNEL_SIZE = 64;
layout (constant_id = 1) const float SSAO_RADIUS = 0.5;

layout (binding = 3) uniform UBOSSAOKernel
{
	vec4 samples[SSAO_KERNEL_SIZE];
} uboSSAOKernel;

layout (binding = 4) uniform UBO 
{
	mat4 invProjection;
} ubo;

layout (location = 0) in vec2 inUV;

layout (location = 0) out float outFragColor;

vec3 ViewPosFromDepth(float depth) {
    // vec4 clipSpacePosition = vec4(inUV * 2.0 - 1.0, depth, 1.0);
    // vec4 viewSpacePosition = ubo.invProjection * clipSpacePosition;

    // // Perspective division
    // viewSpacePosition /= viewSpacePosition.w;

	// for less computations
	vec3 viewSpacePosition = vec3((inUV * 2.f - 1.f) * vec2(ubo.invProjection[0][0], 
										ubo.invProjection[1][1]), -1.f);

    // Perspective division
	viewSpacePosition /= depth * ubo.invProjection[2][3] + ubo.invProjection[3][3];

    return viewSpacePosition.xyz;
}

float linearDepth (float depth) {
	return 1.f / (ubo.invProjection[3][3] + ubo.invProjection[2][3] * depth);
}

void main() 
{
	// Get G-Buffer values
	vec3 fragPos = ViewPosFromDepth(texture(samplerDepth, inUV).r);
	vec3 normal = Decode(texture(samplerNormal, inUV).rg);

	// Get a random vector using a noise lookup
	ivec2 texDim = textureSize(samplerDepth, 0); 
	ivec2 noiseDim = textureSize(ssaoNoise, 0);
	const vec2 noiseUV = vec2(float(texDim.x)/float(noiseDim.x), float(texDim.y)/(noiseDim.y)) * inUV;  
	vec3 randomVec = texture(ssaoNoise, noiseUV).xyz * 2.0 - 1.0;
	
	// Create TBN matrix
	vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
	vec3 bitangent = cross(tangent, normal);
	mat3 TBN = mat3(tangent, bitangent, normal);

	// Calculate occlusion value
	float occlusion = 0.0f;
	// remove banding
	const float bias = 0.025f;
	const mat4 proj = inverse(ubo.invProjection);

	for(int i = 0; i < SSAO_KERNEL_SIZE; i++)
	{		
		vec3 samplePos = TBN * uboSSAOKernel.samples[i].xyz; 
		samplePos = fragPos + samplePos * SSAO_RADIUS; 
		
		// project
		vec4 offset = vec4(samplePos, 1.0f);
		offset = proj * offset; 
		offset.xyz /= offset.w; 
		offset.xyz = offset.xyz * 0.5f + 0.5f; 
		
		float sampleDepth = -linearDepth(texture(samplerDepth, offset.xy).r);

		float rangeCheck = smoothstep(0.0f, 1.0f, SSAO_RADIUS / abs(fragPos.z - sampleDepth));
		occlusion += (sampleDepth >= samplePos.z + bias ? 1.0f : 0.0f) * rangeCheck;           
	}
	occlusion = 1.0 - (occlusion / float(SSAO_KERNEL_SIZE));
	
	outFragColor = occlusion;
}

