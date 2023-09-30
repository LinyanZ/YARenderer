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
    float2 gJitter;
    float2 gPreviousJitter;
};

cbuffer Material : register(b2)
{
    Material Mat;
};

Texture2D AlbedoTexture : register(t0);
Texture2D NormalTexture : register(t1);
Texture2D MetalnessTexture : register(t2);
Texture2D RoughnessTexture : register(t3);

TextureCube irradianceTexture : register(t4);
TextureCube specularTexture : register(t5);
Texture2D specularBRDF_LUT : register(t6);

SamplerState defaultSampler : register(s0);
SamplerState spBRDF_Sampler : register(s1);

struct PixelOut
{
    float4 Albedo : SV_TARGET0;
    float4 NormalVS : SV_TARGET1;
    float Metalness : SV_TARGET2;
    float Roughness : SV_TARGET3;
    float4 Ambient : SV_TARGET4;
};

// Returns number of mipmap levels for specular IBL environment map.
uint QuerySpecularTextureLevels()
{
    uint width, height, levels;
    specularTexture.GetDimensions(0, width, height, levels);
    return levels;
}

PixelOut PS(VertexOut pin)
{
    PixelOut pout;
    
    // Albedo Color
    float3 albedo = Mat.Albedo.rgb;
    if (Mat.HasAlbedoTexture)
    {
        float3 albedoTex = AlbedoTexture.Sample(defaultSampler, pin.TexCoord).rgb;
        albedo *= albedoTex;
    }
    
    // Metalness
    float metalness = Mat.Metalness;
    if (Mat.HasMetalnessTexture)
    {
        metalness = MetalnessTexture.Sample(defaultSampler, pin.TexCoord).r;
    }
    
    // Roughness
    float roughness = Mat.Roughness;
    if (Mat.HasRoughnessTexture)
    {
        roughness = RoughnessTexture.Sample(defaultSampler, pin.TexCoord).r;
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
    
    pout.Albedo = float4(albedo, 1.0f);
    pout.NormalVS = float4(normal, 0.0f);
    pout.Metalness = metalness;
    pout.Roughness = roughness;
    
    //
    // Ambient Lighting (performed in world space)
    // 
    float3 positionW = mul(float4(pin.PositionV, 1.0f), gInvView).xyz;
    float3 normalW = mul(float4(normal, 0.0f), gInvView).xyz;
    
    // Outgoing light direction (vector from world-space fragment position to the "eye").
    float3 V = normalize(gEyePosW.xyz - positionW);
    
    // Angle between surface normal and outgoing light direction.
    float NdotV = max(0.0, dot(normalW, V));
		
	// Specular reflection vector.
    float3 Lr = 2.0 * NdotV * normalW - V;

	// Fresnel reflectance at normal incidence (for metals use albedo color).
    float3 F0 = lerp(Fdielectric, albedo, metalness);
    
    // Ambient lighting (IBL).
    float3 irradiance = irradianceTexture.Sample(defaultSampler, normalW).rgb;
        
    // Calculate Fresnel term for ambient lighting.
	// Since we use pre-filtered cubemap(s) and irradiance is coming from many directions
	// use cosLo instead of angle with light's half-vector (cosLh above).
	// See: https://seblagarde.wordpress.com/2011/08/17/hello-world/
    float3 F = FresnelSchlick(F0, NdotV);

	// Get diffuse contribution factor (as with direct lighting).
    float3 kd = lerp(1.0 - F, 0.0, metalness);

	// Irradiance map contains exitant radiance assuming Lambertian BRDF, no need to scale by 1/PI here either.
    float3 diffuseIBL = kd * albedo * irradiance;

	// Sample pre-filtered specular reflection environment at correct mipmap level.
    uint specularTextureLevels = QuerySpecularTextureLevels();
    float3 specularColor = specularTexture.SampleLevel(defaultSampler, Lr, roughness * specularTextureLevels).rgb;

	// Split-sum approximation factors for Cook-Torrance specular BRDF.
    float2 specularBRDF = specularBRDF_LUT.Sample(spBRDF_Sampler, float2(NdotV, roughness)).rg;

	// Total specular IBL contribution.
    float3 specularIBL = (F0 * specularBRDF.x + specularBRDF.y) * specularColor;

	// Total ambient lighting contribution.
    float3 ambientLighting = diffuseIBL + specularIBL;
    
    pout.Ambient = float4(ambientLighting, 1);
    
    return pout;
}