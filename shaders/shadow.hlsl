#include "common.hlsl"

cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gPrevWorld;
};

cbuffer cbPass : register(b1)
{
    float4x4 gViewProj[4];
};

cbuffer cbIndex : register(b2)
{
    int gIndex;
}

VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut) 0.0f;

    // Transform to world space.
    float4 posW = mul(float4(vin.Position, 1.0f), gWorld);

    // Transform to homogeneous clip space.
    vout.PositionH = mul(posW, gViewProj[gIndex]);
	
    return vout;
}

// This is only used for alpha cut out geometry, so that shadows 
// show up correctly.  Geometry that does not need to sample a
// texture can use a NULL pixel shader for depth pass.
void PS(VertexOut pin)
{
}