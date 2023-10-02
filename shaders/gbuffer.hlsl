#include "constantBuffers.hlsl"
#include "samplers.hlsl"
#include "common.hlsl"

struct VertexIn
{
    float3 Position     : POSITION;
    float3 Normal       : NORMAL;
    float3 Tangent      : TANGENT;
    float3 Bitangent    : BITANGENT;
    float2 TexCoord     : TEXCOORD0;
};

struct VertexOut
{
    float3 PositionW        : POSITION0;        // world space position
    float3 NormalW          : NORMAL0;          // world space normal
    float3 PositionV        : POSITION1;        // view space position
    float3 NormalV          : NORMAL2;          // view space normal
    float3 TangentV         : TANGENT;          // view space tangent
    float3 BitangentV       : BITANGENT;        // view space bitangent
    float2 TexCoord         : TEXCOORD0;        // texture coordinate
    float4 SsaoPosH         : TEXCOORD1;        // texture space ssao texture coordinate
    float4 PositionH        : SV_POSITION;      // clip space position
    float4 CurrPositionH    : POSITION2;        // current clip space position, using SV_POSITION directly results in strange result
    float4 PrevPositionH    : POSITION3;        // previous clip space position
};

struct PixelOut
{
    float4 Albedo : SV_TARGET0;
    float4 NormalVS : SV_TARGET1;
    float Metalness : SV_TARGET2;
    float Roughness : SV_TARGET3;
    float4 Ambient : SV_TARGET4;
    float2 Velocity : SV_TARGET5;
};

// Returns number of mipmap levels for specular IBL environment map.
// uint QuerySpecularTextureLevels()
// {
//     uint width, height, levels;
//     specularTexture.GetDimensions(0, width, height, levels);
//     return levels;
// }

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    vout.TexCoord = vin.TexCoord;

    // transform to world space
    vout.PositionW = mul(float4(vin.Position, 1.0), g_World).xyz;
    vout.NormalW = mul(vin.Normal, (float3x3) g_World);

    float4 prevPositionW = mul(float4(vin.Position, 1.0), g_PrevWorld);
    
    // transform to view space
    float4x4 ModelView = mul(g_World, g_View);
    
    vout.PositionV = mul(float4(vin.Position, 1.0), ModelView).xyz;
    vout.NormalV = mul(vin.Normal, (float3x3) ModelView);
    vout.TangentV = mul(vin.Tangent, (float3x3) ModelView);
    vout.BitangentV = mul(vin.Bitangent, (float3x3) ModelView);
    
    // transform to homogeneous clip space
    vout.PositionH = mul(float4(vout.PositionV, 1.0), g_Proj);
    vout.CurrPositionH = vout.PositionH;
    vout.PrevPositionH = mul(prevPositionW, g_PrevViewProj);
    
    // transform to texture space
    vout.SsaoPosH = mul(float4(vout.PositionV, 1.0), g_ProjTex);
    
    return vout;
}

PixelOut PS(VertexOut pin)
{
    PixelOut pout;
    
    float4 albedo = GetAlbedo(pin.TexCoord);
    float3 normal = GetNormal(pin.NormalV, pin.TangentV, pin.BitangentV, pin.TexCoord);
    float metalness = GetMetalness(pin.TexCoord);
    float roughness = GetRoughness(pin.TexCoord);
    
    pout.Albedo = albedo;
    pout.NormalVS = float4(normal, 0.0f);
    pout.Metalness = metalness;
    pout.Roughness = roughness;
    pout.Ambient = float4(0.05.xxx, 1);  // temporary

    float3 currPosNDC = pin.CurrPositionH.xyz / pin.CurrPositionH.w;
    float3 prevPosNDC = pin.PrevPositionH.xyz / pin.PrevPositionH.w;
    
    pout.Velocity = (currPosNDC.xy - g_Jitter) - (prevPosNDC.xy - g_PreviousJitter);
    
    // //
    // // Ambient Lighting (performed in world space)
    // // 
    // float3 positionW = mul(float4(pin.PositionV, 1.0f), gInvView).xyz;
    // float3 normalW = mul(float4(normal, 0.0f), gInvView).xyz;
    
    // // Outgoing light direction (vector from world-space fragment position to the "eye").
    // float3 V = normalize(gEyePosW.xyz - positionW);
    
    // // Angle between surface normal and outgoing light direction.
    // float NdotV = max(0.0, dot(normalW, V));
		
	// // Specular reflection vector.
    // float3 Lr = 2.0 * NdotV * normalW - V;

	// // Fresnel reflectance at normal incidence (for metals use albedo color).
    // float3 F0 = lerp(Fdielectric, albedo, metalness);
    
    // // Ambient lighting (IBL).
    // float3 irradiance = irradianceTexture.Sample(defaultSampler, normalW).rgb;
        
    // // Calculate Fresnel term for ambient lighting.
	// // Since we use pre-filtered cubemap(s) and irradiance is coming from many directions
	// // use cosLo instead of angle with light's half-vector (cosLh above).
	// // See: https://seblagarde.wordpress.com/2011/08/17/hello-world/
    // float3 F = FresnelSchlick(F0, NdotV);

	// // Get diffuse contribution factor (as with direct lighting).
    // float3 kd = lerp(1.0 - F, 0.0, metalness);

	// // Irradiance map contains exitant radiance assuming Lambertian BRDF, no need to scale by 1/PI here either.
    // float3 diffuseIBL = kd * albedo * irradiance;

	// // Sample pre-filtered specular reflection environment at correct mipmap level.
    // uint specularTextureLevels = QuerySpecularTextureLevels();
    // float3 specularColor = specularTexture.SampleLevel(defaultSampler, Lr, roughness * specularTextureLevels).rgb;

	// // Split-sum approximation factors for Cook-Torrance specular BRDF.
    // float2 specularBRDF = specularBRDF_LUT.Sample(spBRDF_Sampler, float2(NdotV, roughness)).rg;

	// // Total specular IBL contribution.
    // float3 specularIBL = (F0 * specularBRDF.x + specularBRDF.y) * specularColor;

	// // Total ambient lighting contribution.
    // float3 ambientLighting = diffuseIBL + specularIBL;
    
    // pout.Ambient = float4(ambientLighting, 1);
    
    return pout;
}