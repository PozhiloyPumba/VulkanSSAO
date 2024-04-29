#version 450
#extension GL_GOOGLE_include_directive : enable

#include "common.h"

layout (binding = 0) uniform sampler2D samplerDepth;
layout (binding = 1) uniform sampler2D samplerNormal;

layout (binding = 2) uniform UBO 
{
	mat4 invProjection;
} ubo;

layout (constant_id = 0) const float SSAO_RADIUS = 0.5;

#define SSAO_KERNEL_SIZE 32
vec3 samples32[SSAO_KERNEL_SIZE] = {
	{-0.0156128, 0.0240153, 0.00941817},
	{0.00335235, -0.0133425, 0.0100034},
	{-0.000455269, 0.0178843, 0.00648815},
	{0.0547413, 0.0272105, 0.0404667},
	{0.0100051, 0.0165812, 0.00427375},
	{-0.00224504, -0.00183414, 0.00233682},
	{0.015578, -0.0137507, 0.0237539},
	{-0.0401276, 0.0311069, 0.0980725},
	{0.00892654, 0.00918284, 0.0044618},
	{-0.108096, -0.0840795, 0.0612874},
	{0.0144152, -0.048166, 0.0623395},
	{-0.128237, 0.118006, 0.102691},
	{0.0042684, -0.00207643, 0.0306829},
	{-0.104092, -0.0609222, 0.00525195},
	{-0.00597707, 0.0360527, 0.0336474},
	{-0.00828359, 0.0438783, 0.0897723},
	{0.0592099, -0.10195, 0.0591789},
	{-0.175357, -0.0788562, 0.143517},
	{-0.0417744, -0.182259, 0.200682},
	{0.234374, 0.0658437, 0.0399089},
	{0.101492, 0.175797, 0.0296625},
	{-0.116705, 0.076915, 0.278514},
	{-0.0538904, -0.0450951, 0.000333371},
	{0.039051, 0.0326678, 0.0262072},
	{0.00899003, -0.189939, 0.261108},
	{-0.0900529, 0.0204006, 0.252723},
	{0.137526, 0.285846, 0.0720017},
	{-0.0871353, -0.0240037, 0.0145739},
	{0.435368, -0.317575, 0.12516},
	{-0.273164, 0.2708, 0.337032},
	{0.11233, -0.0105726, 0.177544},
	{-0.601136, 0.491848, 0.351547}
};

#define NOISE_SIZE 16
vec2 noise[NOISE_SIZE] = {
	{-0.707796, 0.47263},
	{-0.689759, 0.709843},
	{0.798912, 0.660468},
	{0.929525, 0.32429},
	{0.466104, -0.391427},
	{-0.801209, -0.611753},
	{-0.5834, 0.951646},
	{0.193262, -0.0599244},
	{-0.845354, -0.736363},
	{0.704253, -0.0327553},
	{-0.761869, -0.482157},
	{0.98087, -0.0900248},
	{-0.395518, 0.0884833},
	{-0.958074, 0.545192},
	{-0.669186, -0.558517},
	{-0.66633, 0.419885}
};

layout (location = 0) in vec2 inUV;

layout (location = 0) out float outFragColor;

float linearDepth (float depth) {
	return 1.f / (ubo.invProjection[3][3] + ubo.invProjection[2][3] * depth);
}

float random(vec2 st)
{
    return fract(sin(dot(st.xy, vec2(12.9898,78.233))) * 43758.5453123);
}

void main() 
{
	// Get G-Buffer values
	vec2 packed = texture(samplerNormal, inUV).rg;
	vec3 normal = Decode(packed);

	// Get a random vector using a noise lookup  
	vec3 randomVec = vec3(noise[int(random(gl_FragCoord.xy) * NOISE_SIZE)] * 2.f - 1.f, -1.f);
	
	// Create TBN matrix
	vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
	vec3 bitangent = cross(tangent, normal);
	mat3 TBN = mat3(tangent, bitangent, normal);

	float depth = texture(samplerDepth, inUV).r;
	vec3 fragPos = ViewPosFromDepth(ubo.invProjection, inUV, depth);

	// Calculate occlusion value
	float occlusion = 0.0f;
	// remove banding
	const float bias = 0.025f;

	for(int i = 0; i < SSAO_KERNEL_SIZE; i++) {
		vec3 samplePos = TBN * samples32[i]; 
		samplePos = fragPos + samplePos * SSAO_RADIUS; 
		
		// low computation project
		// vec4 offset = vec4(samplePos, 1.0f);
		// offset = proj * offset; 
		// offset.xyz /= offset.w; 
		// offset.xyz = offset.xyz * 0.5f + 0.5f; 
		
		vec2 offset = samplePos.xy / vec2(ubo.invProjection[0][0], ubo.invProjection[1][1]); 
		offset /= -samplePos.z * 2.f;
 		offset += 0.5f;

		float sampleDepth = -linearDepth(texture(samplerDepth, offset.xy).r);

		float rangeCheck = smoothstep(0.0f, 1.0f, SSAO_RADIUS / abs(fragPos.z - sampleDepth));
		occlusion += mix(0.0f, rangeCheck, sampleDepth >= samplePos.z + bias);         
	}
	occlusion = 1.0 - (occlusion / float(SSAO_KERNEL_SIZE));
	
	outFragColor = occlusion;
}

