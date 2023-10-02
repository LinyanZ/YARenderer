#ifndef __COMMON_HLSL__
#define __COMMON_HLSL__

#include "constantBuffers.hlsl"

float4 GetAlbedo(float2 texCoord)
{
    float4 albedo = float4(g_Material.Albedo.rgb, 1.0);
    if (g_Material.AlbedoTexIndex != -1)
    {
        Texture2D<float4> albedoTexture = ResourceDescriptorHeap[g_Material.AlbedoTexIndex];
        albedo *= albedoTexture.Sample(g_SamplerAnisotropicWrap, texCoord);
    }
    return albedo;
}

float3 GetNormal(float3 normalV, float3 tangentV, float3 bitangentV, float2 texCoord)
{
    float3 normal;
    if (g_Material.NormalTexIndex != -1)
    {
        float3x3 TBN = float3x3(normalize(tangentV),
                                normalize(bitangentV),
                                normalize(normalV));
        
        Texture2D<float4> normalTexture = ResourceDescriptorHeap[g_Material.NormalTexIndex];
        float3 normalTex = normalTexture.Sample(g_SamplerAnisotropicWrap, texCoord).rgb;
        
        // Expand normal range from [0, 1] to [-1, 1].
        normal = normalTex * 2.0 - 1.0;

        // Transform normal from tangent space to view space.
        normal = normalize(mul(normal, TBN));
    }
    else
    {
        normal = normalize(normalV);
    }
    return normal;
}

float GetMetalness(float2 texCoord)
{
    float metalness = g_Material.Metalness;
    if (g_Material.MetalnessTexIndex != -1)
    {
        Texture2D<float4> metalnessTexture = ResourceDescriptorHeap[g_Material.MetalnessTexIndex];
        metalness = metalnessTexture.Sample(g_SamplerAnisotropicWrap, texCoord).b;
    }
    return metalness;
}

float GetRoughness(float2 texCoord)
{
    float roughness = g_Material.Roughness;
    if (g_Material.RoughnessTexIndex != -1)
    {
        Texture2D<float4> roughnessTexture = ResourceDescriptorHeap[g_Material.RoughnessTexIndex];
        roughness = roughnessTexture.Sample(g_SamplerAnisotropicWrap, texCoord).g;
    }
    return roughness;
}

#endif // __COMMON_HLSL__