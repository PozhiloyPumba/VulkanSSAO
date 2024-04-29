#ifndef COMMON
#define COMMON

vec2 OctWrap( vec2 v )
{
    return ( 1.0 - abs( v.yx ) ) * mix(vec2(-1.0), vec2(1.0), greaterThan(v, vec2(0.0)));
}
 
vec2 Encode( vec3 n )
{
    n /= ( abs( n.x ) + abs( n.y ) + abs( n.z ) );
    n.xy = n.z >= 0.0 ? n.xy : OctWrap( n.xy );
    n.xy = n.xy * 0.5 + 0.5;
    return n.xy;
}
 
vec3 Decode( vec2 f )
{
    f = f * 2.0 - 1.0;
    
    // https://twitter.com/Stubbesaurus/status/937994790553227264
    vec3 n = vec3( f.x, f.y, 1.0 - abs( f.x ) - abs( f.y ) );
    float t = clamp(-n.z,0.0,1.0);
    n.xy += mix(vec2(t), vec2(-t), greaterThan(n.xy, vec2(0.0)));
    return normalize( n );
}

vec3 ViewPosFromDepth(mat4 invProj, vec2 inUV, float depth) {
    // vec4 clipSpacePosition = vec4(inUV * 2.0 - 1.0, depth, 1.0);
    // vec4 viewSpacePosition = invProj * clipSpacePosition;

    // // Perspective division
    // viewSpacePosition /= viewSpacePosition.w;

	// for less computations
	vec3 viewSpacePosition = vec3((inUV * 2.f - 1.f) * vec2(invProj[0][0], 
										invProj[1][1]), -1.f);

    // Perspective division
	viewSpacePosition /= depth * invProj[2][3] + invProj[3][3];

    return viewSpacePosition.xyz;
}

uint murmurHash12(uvec2 src) {
    const uint M = 0x5bd1e995u;
    uint h = 1190494759u;
    src *= M; 
    h *= M;
    src ^= src>>24u;
    src *= M;
    h ^= src.x;
    h *= M;
    h ^= src.y;
    h ^= h>>13u;
    h *= M;
    h ^= h>>15u;
    return h;
}

#endif