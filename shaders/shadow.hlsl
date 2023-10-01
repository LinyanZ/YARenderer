#include "common.hlsl"
#include "renderResources.hlsl"
#include "constantBuffers.hlsl"

ConstantBuffer<ShadowRenderResources> g_Resources : register(b6);

struct VertexIn
{
    float3 Position     : POSITION;
    float3 Normal       : NORMAL;
    float3 Tangent      : TANGENT;
    float3 Bitangent    : BITANGENT;
    float2 TexCoord     : TEXCOORD0;
};

float4 VS(VertexIn vin) : SV_POSITION
{
    // Transform to world space.
    float4 posW = mul(float4(vin.Position, 1.0f), g_World);

    // Transform to homogeneous clip space.
    float4 positionH = mul(posW, g_ShadowData.LightViewProj[g_Resources.CascadeIndex]);
	
    return positionH;
}