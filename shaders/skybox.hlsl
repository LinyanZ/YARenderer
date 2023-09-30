#include "renderResources.hlsl"
#include "samplers.hlsl"
#include "constantBuffers.hlsl"

ConstantBuffer<SkyBoxRenderResources> g_Resources : register(b6);

struct VertexIn
{
    float3 PosL : POSITION;
};

struct VertexOut
{
    float3 PosL : POSITION;
    float4 PosH : SV_POSITION;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;

    // Use local vertex position as cubemap lookup vector.
    vout.PosL = vin.PosL;

	// World space remains the same.
    float4 posW = float4(vin.PosL, 1.0f);

	// Always center sky about camera.
    posW.xyz += g_EyePosW;

	// Set z = w so that z/w = 1 (i.e., skydome always on far plane).
    vout.PosH = mul(posW, g_ViewProj).xyww;

    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    TextureCube envMap = ResourceDescriptorHeap[g_Resources.EnvMapTexIndex];
    return envMap.SampleLevel(g_SamplerLinearWrap, pin.PosL, 0);
}