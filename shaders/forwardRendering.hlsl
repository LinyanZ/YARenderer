#include "common.hlsl"
#include "lightingUtils.hlsl"
#include "cascadeShadow.hlsl"
#include "voxelUtils.hlsl"

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
    float2 gJitter;
    float2 gPreviousJitter;
    bool EnableGI;
};

cbuffer Material : register(b2)
{
    Material Mat;
};

cbuffer cbShadowPass : register(b3)
{
    ShadowData gShadowData;
};

Texture2D AlbedoTexture : register(t0);
Texture2D NormalTexture : register(t1);
Texture2D MetalnessTexture : register(t2);
Texture2D RoughnessTexture : register(t3);

TextureCube irradianceTexture : register(t4);
TextureCube specularTexture : register(t5);
Texture2D specularBRDF_LUT : register(t6);

Texture2D gSsaoMap : register(t7);
Texture2DArray gShadowMap : register(t8);

StructuredBuffer<Light> Lights : register(t9);

Texture3D<float4> Voxels : register(t10);

SamplerState defaultSampler : register(s0);
SamplerState spBRDF_Sampler : register(s1);
SamplerComparisonState gsamShadow : register(s2);
SamplerState pointSampler : register(s3);

// Returns number of mipmap levels for specular IBL environment map.
uint QuerySpecularTextureLevels()
{
    uint width, height, levels;
    specularTexture.GetDimensions(0, width, height, levels);
    return levels;
}

float4 PS(VertexOut pin) : SV_TARGET
{
    // Albedo Color
    float3 albedo = Mat.Albedo.xyz;
    float alpha = Mat.Albedo.a;
    if (Mat.HasAlbedoTexture)
    {
        float4 color = AlbedoTexture.Sample(defaultSampler, pin.TexCoord);
        albedo *= color.rgb;
        alpha *= color.a;
    }
    
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
    
    float metalness = (Mat.HasMetalnessTexture) ? 
        MetalnessTexture.Sample(defaultSampler, pin.TexCoord).b : 
        Mat.Metalness;
    float roughness = (Mat.HasRoughnessTexture) ? 
        RoughnessTexture.Sample(defaultSampler, pin.TexCoord).g : 
        Mat.Roughness;
    
    //
    // Direct lighting calculation for analytical lights (performed in view space)
    // 
    
    float2 cascadeShadowResult = CascadeShadowWithPCSS(gShadowMap, gsamShadow, pointSampler, pin.PositionV, gShadowData, gInvView);
    //float shadowFactor = 1;
    float shadowFactor = cascadeShadowResult[0];
    float cascadeIndex = cascadeShadowResult[1];
    
    float3 directLighting = float3(0, 0, 0);
    
    if (gShadowData.ShowCascades)
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
    
    [loop] // Unrolling produces strange behaviour so adding the [loop] attribute.
    for (int i = 0; i < 1; i++)
    {
        // Skip lights that are not enabled.
        if (!Lights[i].Enabled)
            continue;
        
        // Skip point and spot lights that are out of range of the point being shaded.
        if (Lights[i].Type != DIRECTIONAL_LIGHT &&
             length(Lights[i].PositionVS.xyz - pin.PositionV) > Lights[i].Range)
            continue;
 
        switch (Lights[i].Type)
        {
            case DIRECTIONAL_LIGHT:
                directLighting += DoDirectionalLight(Lights[i], albedo, normal, metalness, roughness, pin.PositionV) * shadowFactor;
                break;
            case POINT_LIGHT:
                directLighting += DoPointLight(Lights[i], albedo, normal, metalness, roughness, pin.PositionV);
                break;
            case SPOT_LIGHT:
                directLighting += DoSpotLight(Lights[i], albedo, normal, metalness, roughness, pin.PositionV);
                break;
            default:
                break;
        }
    }
    
    float3 output = directLighting;
    
    if (EnableGI)
    {
        float3 positionW = mul(float4(pin.PositionV, 1.0f), gInvView).xyz;
        float3 normalW = mul(float4(normal, 0.0f), gInvView).xyz;
        float3 V = gEyePosW - positionW;
    
        float4 diffuseIndirectColor = TraceDiffuseCone(Voxels, spBRDF_Sampler, positionW, normalW);
        //float4 specularIndirectColor = TraceSpecularCone(Voxels, spBRDF_Sampler, positionW, normalW, V, roughness);
        
        output.rgb += diffuseIndirectColor.rgb * albedo;
    }
    
    return float4(output, alpha);
    
    
 //   //
 //   // Ambient Lighting (performed in world space)
 //   // 
 //   float3 positionW = mul(float4(pin.PositionV, 1.0f), gInvView).xyz;
 //   float3 normalW = mul(float4(normal, 0.0f), gInvView).xyz;
    
 //   // Outgoing light direction (vector from world-space fragment position to the "eye").
 //   float3 V = normalize(gEyePosW.xyz - positionW);
    
 //   // Angle between surface normal and outgoing light direction.
 //   float NdotV = max(0.0, dot(normalW, V));
		
	//// Specular reflection vector.
 //   float3 Lr = 2.0 * NdotV * normalW - V;

	//// Fresnel reflectance at normal incidence (for metals use albedo color).
 //   float3 F0 = lerp(Fdielectric, albedo, metalness);
    
 //   // Ambient lighting (IBL).
 //   float3 irradiance = irradianceTexture.Sample(defaultSampler, normalW).rgb;
        
 //   // Calculate Fresnel term for ambient lighting.
	//// Since we use pre-filtered cubemap(s) and irradiance is coming from many directions
	//// use cosLo instead of angle with light's half-vector (cosLh above).
	//// See: https://seblagarde.wordpress.com/2011/08/17/hello-world/
 //   float3 F = FresnelSchlick(F0, NdotV);

	//// Get diffuse contribution factor (as with direct lighting).
 //   float3 kd = lerp(1.0 - F, 0.0, metalness);

	//// Irradiance map contains exitant radiance assuming Lambertian BRDF, no need to scale by 1/PI here either.
 //   float3 diffuseIBL = kd * albedo * irradiance;

	//// Sample pre-filtered specular reflection environment at correct mipmap level.
 //   uint specularTextureLevels = QuerySpecularTextureLevels();
 //   float3 specularColor = specularTexture.SampleLevel(defaultSampler, Lr, roughness * specularTextureLevels).rgb;

	//// Split-sum approximation factors for Cook-Torrance specular BRDF.
 //   float2 specularBRDF = specularBRDF_LUT.Sample(spBRDF_Sampler, float2(NdotV, roughness)).rg;

	//// Total specular IBL contribution.
 //   float3 specularIBL = (F0 * specularBRDF.x + specularBRDF.y) * specularColor;

	//// Total ambient lighting contribution.
 //   float3 ambientLighting = diffuseIBL + specularIBL;
    
 //   // Sample ssao map.
 //   pin.SsaoPosH /= pin.SsaoPosH.w;
 //   float ambientAccess = gSsaoMap.Sample(spBRDF_Sampler, pin.SsaoPosH.xy, 0.0f).r;
    
 //   return float4(directLighting + ambientLighting * ambientAccess, alpha);
}