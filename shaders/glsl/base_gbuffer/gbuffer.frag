#version 450
#extension GL_GOOGLE_include_directive : enable

#include "common.h"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inColor;

layout (location = 0) out vec2 outNormal;
layout (location = 1) out vec4 outAlbedo;

layout (set = 0, binding = 0) uniform UBO 
{
	mat4 projection;
	mat4 model;
	mat4 view;
	float nearPlane;
	float farPlane;
} ubo;

layout (set = 1, binding = 0) uniform sampler2D samplerColormap;

void main()
{
	outNormal = Encode(inNormal);
	outAlbedo = texture(samplerColormap, inUV) * vec4(inColor, 1.0);
}