#include "constantBuffers.hlsl"
#include "lightingUtils.hlsl"
#include "samplers.hlsl"
#include "cascadedShadow.hlsl"
#include "voxelUtils.hlsl"

struct Resources
{
	uint AlbedoTexIndex;
	uint NormalTexIndex;
	uint MetalnessTexIndex;
	uint RoughnessTexIndex;
	uint AmbientTexIndex;
	uint DepthTexIndex;
	uint ShadowMapTexIndex;
    uint VoxelTexIndex;
    uint IrradianceMapIndex;
    uint SpecularMapIndex;
    uint BRDFLUTIndex;
};

ConstantBuffer<Resources> g_Resources : register(b6);

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

    int cascadeIndex = GetCascadeIndex(positionV);
    float shadowFactor = CascadedShadowWithPCSS(shadowMap, cascadeIndex, positionV);
    
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

    //
    // Ambient Lighting (performed in world space)
    // 
    float3 positionW = mul(float4(positionV, 1.0f), g_InvView).xyz;
    float3 normalW = mul(float4(normal, 0.0f), g_InvView).xyz;
        
    // Outgoing light direction (vector from world-space fragment position to the "eye").
    float3 V = normalize(g_EyePosW - positionW);

    if (g_EnableGI)
    {
        Texture3D voxels = ResourceDescriptorHeap[g_Resources.VoxelTexIndex];
    
        float4 diffuseIndirectColor = TraceDiffuseCone(voxels, g_SamplerLinearClamp, positionW, normalW);
        // float4 specularIndirectColor = TraceSpecularCone(voxels, g_SamplerLinearClamp, positionW, normalW, V, roughness);
        
        ambient += diffuseIndirectColor.rgb;
        // ambient += specularIndirectColor.rgb;
    }

    if (g_EnableIBL)
    {
        TextureCube irradianceTexture = ResourceDescriptorHeap[g_Resources.IrradianceMapIndex];
        TextureCube specularTexture = ResourceDescriptorHeap[g_Resources.SpecularMapIndex];
        Texture2D specularBRDFLUT = ResourceDescriptorHeap[g_Resources.BRDFLUTIndex];

        // Angle between surface normal and outgoing light direction.
        float NdotV = max(0.0, dot(normalW, V));
                
        // Specular reflection vector.
        float3 Lr = 2.0 * NdotV * normalW - V;

        // Fresnel reflectance at normal incidence (for metals use albedo color).
        float3 F0 = lerp(Fdielectric, albedo, metalness);
            
        // Ambient lighting (IBL).
        float3 irradiance = irradianceTexture.Sample(g_SamplerAnisotropicWrap, normalW).rgb;
                
        // Calculate Fresnel term for ambient lighting.
        // Since we use pre-filtered cubemap(s) and irradiance is coming from many directions
        // use NdotV instead of angle with light's half-vector.
        // See: https://seblagarde.wordpress.com/2011/08/17/hello-world/
        float3 F = FresnelSchlick(F0, NdotV);

        // Get diffuse contribution factor (as with direct lighting).
        float3 kd = lerp(1.0 - F, 0.0, metalness);

        // Irradiance map contains exitant radiance assuming Lambertian BRDF, no need to scale by 1/PI here either.
        float3 diffuseIBL = kd * albedo * irradiance;

        // Sample pre-filtered specular reflection environment at correct mipmap level.
        uint specularTextureWidth, specularTextureHeight, specularTextureLevels;
        specularTexture.GetDimensions(0, specularTextureWidth, specularTextureHeight, specularTextureLevels);
        float3 specularColor = specularTexture.SampleLevel(g_SamplerAnisotropicWrap, Lr, roughness * specularTextureLevels).rgb;

        // Split-sum approximation factors for Cook-Torrance specular BRDF.
        float2 specularBRDF = specularBRDFLUT.Sample(g_SamplerLinearClamp, float2(NdotV, roughness)).rg;

        // Total specular IBL contribution.
        float3 specularIBL = (F0 * specularBRDF.x + specularBRDF.y) * specularColor;

        // Total ambient lighting contribution.
        ambient += (diffuseIBL + specularIBL);
        
        // Sample ssao map.
        // pin.SsaoPosH /= pin.SsaoPosH.w;
        // float ambientAccess = gSsaoMap.Sample(spBRDF_Sampler, pin.SsaoPosH.xy, 0.0f).r;
    }

    return float4(directLighting + ambient * albedo, alpha);
}