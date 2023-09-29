static const float PI = 3.141592;
static const float Epsilon = 0.00001;

// Constant normal incidence Fresnel factor for all dielectrics.
static const float3 Fdielectric = 0.04;

cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
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
};

Texture2D albedoTexture : register(t0);
Texture2D normalTexture : register(t1);
Texture2D metalnessTexture : register(t2);
Texture2D roughnessTexture : register(t3);

TextureCube irradianceTexture : register(t4);
TextureCube specularTexture : register(t5);
Texture2D specularBRDF_LUT : register(t6);

Texture2D gSsaoMap : register(t7);

SamplerState defaultSampler : register(s0);
SamplerState spBRDF_Sampler : register(s1);

struct VertexIn
{
    float3 PosL : POSITION;
    float3 Normal : NORMAL;
    float3 Tangent : TANGENT;
    float3 Bitangent : BITANGENT;
    float2 TexCoord : TEXCOORD;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float2 TexCoord : TEXCOORD;
	float3x3 TangentBasis : TBASIS;
    float4 SsaoPosH   : POSITION1;
};


// GGX/Towbridge-Reitz normal distribution function.
// Uses Disney's reparametrization of alpha = roughness^2.
float ndfGGX(float cosLh, float roughness)
{
    float alpha = roughness * roughness;
    float alphaSq = alpha * alpha;

    float denom = (cosLh * cosLh) * (alphaSq - 1.0) + 1.0;
    return alphaSq / (PI * denom * denom);
}

// Single term for separable Schlick-GGX below.
float gaSchlickG1(float cosTheta, float k)
{
    return cosTheta / (cosTheta * (1.0 - k) + k);
}

// Schlick-GGX approximation of geometric attenuation function using Smith's method.
float gaSchlickGGX(float cosLi, float cosLo, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0; // Epic suggests using this roughness remapping for analytic lights.
    return gaSchlickG1(cosLi, k) * gaSchlickG1(cosLo, k);
}

// Shlick's approximation of the Fresnel factor.
float3 fresnelSchlick(float3 F0, float cosTheta)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

// Returns number of mipmap levels for specular IBL environment map.
uint querySpecularTextureLevels()
{
    uint width, height, levels;
    specularTexture.GetDimensions(0, width, height, levels);
    return levels;
}

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    vout.TexCoord = float2(vin.TexCoord.x, 1.0f - vin.TexCoord.y);

	// Transform to world space.
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW.xyz;

	// Transform to homogeneous clip space.
    vout.PosH = mul(float4(vout.PosW, 1.0f), gViewProj);
    
    // Generate projective tex-coords to project SSAO map onto scene.
    vout.SsaoPosH = mul(posW, gViewProjTex);
    
    // Pass tangent space basis vectors (for normal mapping).
    float3x3 TBN = float3x3(vin.Tangent, vin.Bitangent, vin.Normal);
    vout.TangentBasis = mul(transpose(TBN), (float3x3) gWorld);
    
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    // Sample input textures to get shading model params.
    float3 albedo = albedoTexture.Sample(defaultSampler, pin.TexCoord).rgb;
    float metalness = metalnessTexture.Sample(defaultSampler, pin.TexCoord).r;
    float roughness = roughnessTexture.Sample(defaultSampler, pin.TexCoord).r;
    
	// Outgoing light direction (vector from world-space fragment position to the "eye").
    float3 Lo = normalize(gEyePosW - pin.PosW);

	// Get current fragment's normal and transform to world space.
    float3 N = normalize(2.0 * normalTexture.Sample(defaultSampler, pin.TexCoord).rgb - 1.0);
    N = normalize(mul(pin.TangentBasis, N));
    
	// Angle between surface normal and outgoing light direction.
    float cosLo = max(0.0, dot(N, Lo));
		
	// Specular reflection vector.
    float3 Lr = 2.0 * cosLo * N - Lo;

	// Fresnel reflectance at normal incidence (for metals use albedo color).
    float3 F0 = lerp(Fdielectric, albedo, metalness);

	// Direct lighting calculation for analytical lights.
    float3 directLighting = 0.0;
    
    float3 lightDirs[3] =
    {
        float3(-1.0f, 0.0f, 0.0f),
        float3(0.7f, 0.0f, 0.3f),
        float3(0.0f, -1.0f, 0.0f)
    };
    float3 lightRadiance = float3(0.8f, 0.93f, 1.0f);
        
    for (uint i = 0; i < 3; i++)
    {
        float3 Li = -lightDirs[i];
        float3 Lradiance = lightRadiance;

        // Half-vector between Li and Lo.
        float3 Lh = normalize(Li + Lo);

        // Calculate angles between surface normal and various light vectors.
        float cosLi = max(0.0, dot(N, Li));
        float cosLh = max(0.0, dot(N, Lh));

        // Calculate Fresnel term for direct lighting. 
        float3 F = fresnelSchlick(F0, max(0.0, dot(Lh, Lo)));
        // Calculate normal distribution for specular BRDF.
        float D = ndfGGX(cosLh, roughness);
        // Calculate geometric attenuation for specular BRDF.
        float G = gaSchlickGGX(cosLi, cosLo, roughness);

        // Diffuse scattering happens due to light being refracted multiple times by a dielectric medium.
        // Metals on the other hand either reflect or absorb energy, so diffuse contribution is always zero.
        // To be energy conserving we must scale diffuse BRDF contribution based on Fresnel factor & metalness.
        float3 kd = lerp(float3(1, 1, 1) - F, float3(0, 0, 0), metalness);

        // Lambert diffuse BRDF.
        // We don't scale by 1/PI for lighting & material units to be more convenient.
        // See: https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
        float3 diffuseBRDF = kd * albedo;

        // Cook-Torrance specular microfacet BRDF.
        float3 specularBRDF = (F * D * G) / max(Epsilon, 4.0 * cosLi * cosLo);

        // Total contribution for this light.
        directLighting += (diffuseBRDF + specularBRDF) * Lradiance * cosLi;
    }
    

	// Ambient lighting (IBL).
    float3 irradiance = irradianceTexture.Sample(defaultSampler, N).rgb;
        
    // Calculate Fresnel term for ambient lighting.
	// Since we use pre-filtered cubemap(s) and irradiance is coming from many directions
	// use cosLo instead of angle with light's half-vector (cosLh above).
	// See: https://seblagarde.wordpress.com/2011/08/17/hello-world/
    float3 F = fresnelSchlick(F0, cosLo);

	// Get diffuse contribution factor (as with direct lighting).
    float3 kd = lerp(1.0 - F, 0.0, metalness);

	// Irradiance map contains exitant radiance assuming Lambertian BRDF, no need to scale by 1/PI here either.
    float3 diffuseIBL = kd * albedo * irradiance;

	// Sample pre-filtered specular reflection environment at correct mipmap level.
    uint specularTextureLevels = querySpecularTextureLevels();
    float3 specularIrradiance = specularTexture.SampleLevel(defaultSampler, Lr, roughness * specularTextureLevels).rgb;

	// Split-sum approximation factors for Cook-Torrance specular BRDF.
    float2 specularBRDF = specularBRDF_LUT.Sample(spBRDF_Sampler, float2(cosLo, roughness)).rg;

	// Total specular IBL contribution.
    float3 specularIBL = (F0 * specularBRDF.x + specularBRDF.y) * specularIrradiance;

	// Total ambient lighting contribution.
    float3 ambientLighting = diffuseIBL + specularIBL;
    
    // Sample ssao map.
    pin.SsaoPosH /= pin.SsaoPosH.w;
    float ambientAccess = gSsaoMap.Sample(spBRDF_Sampler, pin.SsaoPosH.xy, 0.0f).r;
	
    // Final fragments color.
    return float4(directLighting + ambientLighting * ambientAccess, 1.0);
}