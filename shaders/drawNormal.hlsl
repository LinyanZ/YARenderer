#include "common.hlsl"
#include "lightingUtils.hlsl"

cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gPrevWorld;
};

cbuffer cbPass : register(b1)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gPrevViewProj;
    float4x4 gInvViewProj;
    float4x4 gViewProjTex;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
};

cbuffer Material : register(b2)
{
    Material Mat;
};

Texture2D NormalTexture : register(t0);

SamplerState defaultSampler : register(s0);

float4 PS(VertexOut pin) : SV_Target
{
	// Normal Mapping
    float3 normal;
    if (Mat.HasNormalTexture)
    {
        float3x3 TBN = float3x3(normalize(pin.TangentV),
                                normalize(pin.BitangentV),
                                normalize(pin.NormalV));
        
        float3 normalTex = NormalTexture.Sample(defaultSampler, pin.TexCoord).rgb;
        
        // Expand normal range from [0, 1] to [-1, 1].
        normal = normalTex * 2.0 - 1.0;

        // Transform normal from tangent space to view space.
        normal = normalize(mul(normal, TBN));
    }
    else
    {
        normal = normalize(pin.NormalV);
    }

    return float4(normal, 1.0);
}