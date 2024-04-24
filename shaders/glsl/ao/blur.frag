#version 450

layout (binding = 0) uniform sampler2D samplerAO;
layout (binding = 1) uniform sampler2D depth;

layout (binding = 2) uniform UBO 
{
	int depth_check;
	float depth_range;
	float nearPlane;
	float farPlane;
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
	const int blurRange = 2;
	int n = 0;
	vec2 texelSize = 1.0 / vec2(textureSize(samplerAO, 0));
	float result = 0.0;
	if (bool(ubo.depth_check)) {
		float sampleDepth = linearDepth(texture(depth, inUV).r);

		for (int x = -blurRange; x < blurRange; x++) 
		{
			for (int y = -blurRange; y < blurRange; y++) 
			{
				vec2 offset = vec2(float(x), float(y)) * texelSize;
				float d = linearDepth(texture(depth, inUV + offset).r);
				int choose = int(abs(sampleDepth - d) < ubo.depth_range);
				result += texture(samplerAO, inUV + offset).r * choose;
				n += choose;
			}
		}
	} else {
		for (int x = -blurRange; x < blurRange; x++) 
		{
			for (int y = -blurRange; y < blurRange; y++) 
			{
				vec2 offset = vec2(float(x), float(y)) * texelSize;
				result += texture(samplerAO, inUV + offset).r;
				n++;
			}
		}
	}
	outFragColor = result / (float(n));
}