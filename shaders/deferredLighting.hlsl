#include "renderResources.hlsl"
#include "constantBuffers.hlsl"
#include "lightingUtils.hlsl"
#include "samplers.hlsl"
#include "cascadedShadow.hlsl"

ConstantBuffer<DeferredLightingRenderResources> g_Resources : register(b6);

struct VertexOut
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD;
};

VertexOut VS(uint vertexID : SV_VertexID)
{
    VertexOut vout;
    
    // draw a triangle that covers the entire screen
    const float2 tex = float2(uint2(vertexID, vertexID << 1) & 2);
    vout.Position = float4(lerp(float2(-1, 1), float2(1, -1), tex), 0, 1);
    vout.TexCoord = tex;
    
    return vout;
}

// convert screen space coordinates to view space.
float4 ScreenToView(float4 screen)
{
    // convert to normalized texture coordinates
    float2 texCoord = screen.xy * g_InvRenderTargetSize;
 
    // convert to clip space
    float4 clip = float4(float2(texCoord.x, 1.0f - texCoord.y) * 2.0f - 1.0f, screen.z, screen.w);

    // view space position
    float4 view = mul(clip, g_InvProj);
    view /= view.w;

    return view;
}

float4 PS(VertexOut pin) : SV_Target
{
    int2 texCoord = pin.Position.xy;

    Texture2D<float4> albedoTex = ResourceDescriptorHeap[g_Resources.AlbedoTexIndex];
    Texture2D<float4> normalTex = ResourceDescriptorHeap[g_Resources.NormalTexIndex];
    Texture2D<float> metalnessTex = ResourceDescriptorHeap[g_Resources.MetalnessTexIndex];
    Texture2D<float> roughnessTex = ResourceDescriptorHeap[g_Resources.RoughnessTexIndex];
    Texture2D<float4> ambientTex = ResourceDescriptorHeap[g_Resources.AmbientTexIndex];
    Texture2D<float> depthTex = ResourceDescriptorHeap[g_Resources.DepthTexIndex];
    Texture2DArray shadowMap = ResourceDescriptorHeap[g_Resources.ShadowMapTexIndex];

    float4 albedoCol = albedoTex.Load(int3(texCoord, 0));
    float3 albedo = albedoCol.rgb;
    float alpha = albedoCol.a;

    float3 normal = normalTex.Load(int3(texCoord, 0)).rgb;
    float metalness = metalnessTex.Load(int3(texCoord, 0));
    float roughness = roughnessTex.Load(int3(texCoord, 0));
    float3 ambient = ambientTex.Load(int3(texCoord, 0)).rgb;

    float depth = depthTex.Load(int3(texCoord, 0));
    float3 positionV = ScreenToView(float4(texCoord, depth, 1.0f)).xyz;

    float2 cascadeShadowResult = CascadeShadowWithPCSS(shadowMap, positionV);
    float shadowFactor = cascadeShadowResult[0];
    float cascadeIndex = cascadeShadowResult[1];
    
    float3 directLighting = float3(0, 0, 0);
    
    if (g_ShadowData.ShowCascades)
    {
        if (cascadeIndex == 0)
            directLighting += float3(0.8, 0, 0);
        else if (cascadeIndex == 1)
            directLighting += float3(0, 0.8, 0);
        else if (cascadeIndex == 2)
            directLighting += float3(0, 0, 0.8);
        else if (cascadeIndex == 3)
            directLighting += float3(0.8, 0.8, 0);
    }

    [loop] // unrolling produces strange behaviour so adding the [loop] attribute
    for (int i = 0; i < 1; i++)
    {
        // skip lights that are not enabled
        if (!LightCB[i].Enabled)
            continue;
        
        // skip point and spot lights that are out of range of the point being shaded
        if (LightCB[i].Type != DIRECTIONAL_LIGHT &&
            length(LightCB[i].PositionVS.xyz - positionV) > LightCB[i].Range)
            continue;
 
        switch (LightCB[i].Type)
        {
            case DIRECTIONAL_LIGHT:
                directLighting += DoDirectionalLight(LightCB[i], albedo, normal, metalness, roughness, positionV) * shadowFactor;
                break;
            case POINT_LIGHT:
                directLighting += DoPointLight(LightCB[i], albedo, normal, metalness, roughness, positionV);
                break;
            case SPOT_LIGHT:
                directLighting += DoSpotLight(LightCB[i], albedo, normal, metalness, roughness, positionV);
                break;
            default:
                break;
        }
    }

    return float4(directLighting + ambient * albedo, alpha);
}