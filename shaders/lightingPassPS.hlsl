#include "lightingUtils.hlsl"

cbuffer cbLightIndex : register(b0)
{
    uint gLightIndex;
};

cbuffer cbPass : register(b1)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
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
    float2 gJitter;
    float2 gPreviousJitter;
};

StructuredBuffer<Light> Lights : register(t0);

Texture2D AlbedoTexture : register(t1);
Texture2D NormalTexture : register(t2);
Texture2D MetalnessTexture : register(t3);
Texture2D RoughnessTexture : register(t4);
Texture2D DepthTexture : register(t5);

// Convert clip space coordinates to view space
float4 ClipToView(float4 clip)
{
    // View space position.
    float4 view = mul(clip, gInvProj);
    // Perspective projection.
    view = view / view.w;
 
    return view;
}
 
// Convert screen space coordinates to view space.
float4 ScreenToView(float4 screen)
{
    // Convert to normalized texture coordinates
    float2 texCoord = screen.xy * gInvRenderTargetSize;
 
    // Convert to clip space
    float4 clip = float4(float2(texCoord.x, 1.0f - texCoord.y) * 2.0f - 1.0f, screen.z, screen.w);
    return ClipToView(clip);
}

float4 PS(VertexOut pin) : SV_TARGET
{
    int2 texCoord = pin.PositionH.xy;
    
    float3 albedo = AlbedoTexture.Load(int3(texCoord, 0)).xyz;
    float3 normal = NormalTexture.Load(int3(texCoord, 0)).xyz;
    float metalness = MetalnessTexture.Load(int3(texCoord, 0)).r;
    float roughness = RoughnessTexture.Load(int3(texCoord, 0)).r;
    
    float depth = DepthTexture.Load(int3(texCoord, 0)).r;
    float4 positionVS = ScreenToView(float4(texCoord, depth, 1.0f));
    
    // Direct lighting calculation for analytical lights.
    float3 directLighting = float3(0, 0, 0);
    
    switch (Lights[gLightIndex].Type)
    {
        case DIRECTIONAL_LIGHT:
            directLighting += DoDirectionalLight(Lights[gLightIndex], albedo, normal, metalness, roughness, positionVS.xyz);
            break;
        case POINT_LIGHT:
            directLighting += DoPointLight(Lights[gLightIndex], albedo, normal, metalness, roughness, positionVS.xyz);
            break;
        case SPOT_LIGHT:
            directLighting += DoSpotLight(Lights[gLightIndex], albedo, normal, metalness, roughness, positionVS.xyz);
            break;
        default:
            break;
    }
    
    return float4(directLighting, 1.0);
}