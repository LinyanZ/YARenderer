#include "common.hlsl"

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
    float4x4 gProjTex;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
    float2 gJitter;
    float2 gPreviousJitter;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    vout.TexCoord = vin.TexCoord;
    
    float4x4 ModelView = mul(gWorld, gView);
    
    // Transform to view space.
    vout.PositionV = mul(float4(vin.Position, 1.0), ModelView).rgb;
    vout.NormalV = mul(vin.Normal, (float3x3) ModelView);
    vout.TangentV = mul(vin.Tangent, (float3x3) ModelView);
    vout.BitangentV = mul(vin.Bitangent, (float3x3) ModelView);
    
    // Transform to homogeneous clip space.
    vout.PositionH = mul(float4(vout.PositionV, 1.0), gProj);
    
    // Transform to texture space.
    vout.SsaoPosH = mul(float4(vout.PositionV, 1.0), gProjTex);
    
    return vout;
}