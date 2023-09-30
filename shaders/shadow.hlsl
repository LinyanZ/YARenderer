#include "common.hlsl"
#include "renderResources.hlsl"
#include "constantBuffers.hlsl"

ConstantBuffer<ShadowRenderResources> g_Resources : register(b6);

VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut) 0.0f;

    // Transform to world space.
    float4 posW = mul(float4(vin.Position, 1.0f), g_World);

    // Transform to homogeneous clip space.
    vout.PositionH = mul(posW, g_ShadowData.LightViewProj[g_Resources.CascadeIndex]);
	
    return vout;
}