#include "common.hlsl"
#include "cascadeShadow.hlsl"

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

cbuffer cbShadowPass : register(b3)
{
    ShadowData gShadowData;
};

Texture2D AlbedoTexture : register(t0);
Texture2D NormalTexture : register(t1);
Texture2D MetalnessTexture : register(t2);
Texture2D RoughnessTexture : register(t3);
Texture2DArray gShadowMap : register(t4);

StructuredBuffer<Light> Lights : register(t5);

RWTexture3D<uint> gVoxelizerAlbedo : register(u0);

SamplerState defaultSampler : register(s0);
SamplerState spBRDF_Sampler : register(s1);
SamplerComparisonState gsamShadow : register(s2);
SamplerState pointSampler : register(s3);


float3 DoDirectLighting(Light light, float3 albedo, float3 normal, float metalness, float roughness, float3 L, float3 V)
{
    // Calculate angles between surface normal and various light vectors.
    float NdotL = max(0.0, dot(normal, L));

    // Diffuse scattering happens due to light being refracted multiple times by a dielectric medium.
    // Metals on the other hand either reflect or absorb energy, so diffuse contribution is always zero.
    // To be energy conserving we must scale diffuse BRDF contribution based on Fresnel factor & metalness.
    float3 kd = lerp(float3(1, 1, 1), float3(0, 0, 0), metalness);

    // Lambert diffuse BRDF.
    // We don't scale by 1/PI for lighting & material units to be more convenient.
    // See: https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
    float3 diffuseBRDF = kd * albedo;
    
    return diffuseBRDF * light.Color.xyz * NdotL;
}

float3 DoDirectionalLight(Light light, float3 albedo, float3 normal, float metalness, float roughness, float3 PositionV)
{
    // Everything is in view space.
    float4 eyePos = { 0, 0, 0, 1 };
    
    float3 V = normalize(eyePos.xyz - PositionV);
    float3 L = normalize(-light.DirectionVS.xyz);
    
    return DoDirectLighting(light, albedo, normal, metalness, roughness, L, V) * light.Intensity;
}

float DoAttenuation(Light light, float distance)
{
    return 1.0f - smoothstep(light.Range * 0.75f, light.Range, distance);
}

float3 DoPointLight(Light light, float3 albedo, float3 normal, float metalness, float roughness, float3 PositionV)
{
    // Everything is in view space.
    float4 eyePos = { 0, 0, 0, 1 };
    
    float3 V = normalize(eyePos.xyz - PositionV);
    
    float3 L = light.PositionVS.xyz - PositionV;
    float distance = length(L);
    L /= distance;
    
    float attenuation = DoAttenuation(light, distance);
    
    return DoDirectLighting(light, albedo, normal, metalness, roughness, L, V) * attenuation * light.Intensity;
}

float DoSpotCone(Light light, float3 L)
{
    // If the cosine angle of the light's direction 
    // vector and the vector from the light source to the point being 
    // shaded is less than minCos, then the spotlight contribution will be 0.
    float minCos = cos(radians(light.SpotlightAngle));
    // If the cosine angle of the light's direction vector
    // and the vector from the light source to the point being shaded
    // is greater than maxCos, then the spotlight contribution will be 1.
    float maxCos = lerp(minCos, 1, 0.5f);
    float cosAngle = dot(light.DirectionVS.xyz, -L);
    // Blend between the minimum and maximum cosine angles.
    return smoothstep(minCos, maxCos, cosAngle);
}

float3 DoSpotLight(Light light, float3 albedo, float3 normal, float metalness, float roughness, float3 PositionV)
{
    // Everything is in view space.
    float4 eyePos = { 0, 0, 0, 1 };
    
    float3 V = normalize(eyePos.xyz - PositionV);
    
    float3 L = light.PositionVS.xyz - PositionV;
    float distance = length(L);
    L /= distance;
    
    float attenuation = DoAttenuation(light, distance);
    float spotIntensity = DoSpotCone(light, L);
    
    return DoDirectLighting(light, albedo, normal, metalness, roughness, L, V) * attenuation * spotIntensity * light.Intensity;
}


struct GeometryInOut
{
    float3 PositionW    : POSITION0;        // World space position.
    float3 PositionV    : POSITION1;        // View space position.
    float3 NormalW      : NORMAL0;          // World space normal.
    float3 NormalV      : NORMAL1;          // View space normal.
    float3 TangentV     : TANGENT;          // View space tangent.
    float3 BitangentV   : BITANGENT;        // View space bitangent.
    float2 TexCoord     : TEXCOORD0;        // Texture coordinate.
    float4 PositionH    : SV_POSITION;      // Clip space position.
};

GeometryInOut VS(VertexIn vin)
{
    GeometryInOut vout;
    vout.TexCoord = vin.TexCoord;
    
    // Transform the position into world space as we will use 
    // this to index into the voxel grid and write to it
    vout.PositionW = mul(float4(vin.Position, 1.0f), gWorld).xyz;
    vout.NormalW = mul(vin.Normal, (float3x3) gWorld);
    
    float4x4 ModelView = mul(gWorld, gView);
    
    // Transform to view space.
    vout.PositionV = mul(float4(vin.Position, 1.0), ModelView).rgb;
    vout.NormalV = mul(vin.Normal, (float3x3) ModelView);
    vout.TangentV = mul(vin.Tangent, (float3x3) ModelView);
    vout.BitangentV = mul(vin.Bitangent, (float3x3) ModelView);
    
    vout.PositionH = float4(vout.PositionW, 1);
    
    return vout;
}

[maxvertexcount(3)]
void GS(triangle GeometryInOut input[3], inout TriangleStream<GeometryInOut> triStream)
{
    // Select the greatest component of the face normal
    float3 posW0 = input[0].PositionW;
    float3 posW1 = input[1].PositionW;
    float3 posW2 = input[2].PositionW;
    float3 faceNormal = abs(cross(posW1 - posW0, posW2 - posW0)); 

    uint maxI = faceNormal[1] > faceNormal[0] ? 1 : 0;
    maxI = faceNormal[2] > faceNormal[maxI] ? 2 : maxI;
    
    GeometryInOut output[3];
    
    for (uint i = 0; i < 3; i++)
    {
        // World space -> Voxel grid space
        output[i].PositionH.xyz = (input[i].PositionH.xyz - VOXEL_GRID_WORLD_POS) / VOXEL_GRID_SIZE;
        
        // Project to the dominant axis orthogonally
        [flatten]
        switch (maxI)
        {
            case 0:
                output[i].PositionH.xyz = float4(output[i].PositionH.yzx, 1.0f).xyz;
                break;
            case 1:
                output[i].PositionH.xyz = float4(output[i].PositionH.zxy, 1.0f).xyz;
                break;
            case 2:
                //output[i].PositionH.xyz = float4(output[i].PositionH.xyz, 1.0f);
                break;
        }
        
        // Voxel grid space -> Clip space
        output[i].PositionH.xy /= VOXEL_DIMENSION;
        output[i].PositionH.zw = 1;
        
        output[i].PositionW = input[i].PositionW / VOXEL_DIMENSION / VOXEL_GRID_SIZE;
        output[i].PositionV = input[i].PositionV;
        output[i].NormalW = input[i].NormalW;
        output[i].NormalV = input[i].NormalV;
        output[i].TangentV = input[i].TangentV;
        output[i].BitangentV = input[i].BitangentV;
        output[i].TexCoord = input[i].TexCoord;
        
        triStream.Append(output[i]);
    }
}

// ref: file:///C:/Users/LINYAN-DESKTOP/Desktop/OpenGLInsights.pdf Chapter 22
uint convVec4ToRGBA8(float4 val)
{
    return (uint(val.w) & 0x000000FF) << 24U | (uint(val.z) & 0x000000FF) << 16U | (uint(val.y) & 0x000000FF) << 8U | (uint(val.x) & 0x000000FF);
}

float4 convRGBA8ToVec4(uint val)
{
    float4 re = float4(float((val & 0x000000FF)), float((val & 0x0000FF00) >> 8U), float((val & 0x00FF0000) >> 16U), float((val & 0xFF000000) >> 24U));
    return clamp(re, float4(0.0, 0.0, 0.0, 0.0), float4(255.0, 255.0, 255.0, 255.0));
}

void ImageAtomicRGBA8Avg(RWTexture3D<uint> image, uint3 coord, float4 val)
{
    val.rgb *= 255.0f;
    uint newVal = convVec4ToRGBA8(val);
    
    uint prevStoredVal = 0;
    uint curStoredVal;
    
    InterlockedCompareExchange(image[coord], prevStoredVal, newVal, curStoredVal);
    while (curStoredVal != prevStoredVal)
    {
        prevStoredVal = curStoredVal;
        float4 rval = convRGBA8ToVec4(curStoredVal);
        rval.xyz *= rval.w;
        
        float4 curValF = rval + val;
        curValF.xyz /= curValF.w;
        
        newVal = convVec4ToRGBA8(curValF);
        InterlockedCompareExchange(image[coord], prevStoredVal, newVal, curStoredVal);
    }
}

void PS(GeometryInOut pin)
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
    
    uint3 texIndex = uint3(
        (pin.PositionW.x * 0.5 + 0.5f) * VOXEL_DIMENSION,
        (pin.PositionW.y * -0.5 + 0.5f) * VOXEL_DIMENSION,
        (pin.PositionW.z * 0.5 + 0.5f) * VOXEL_DIMENSION
    );    
    
    if (all(texIndex < VOXEL_DIMENSION) && all(texIndex >= 0))
    {
        //
        // Direct lighting calculation for analytical lights (performed in view space)
        // 
        
        float ends[] = {
            gShadowData.CascadeEnds[0].x,
            gShadowData.CascadeEnds[0].y,
            gShadowData.CascadeEnds[0].z,
            gShadowData.CascadeEnds[0].w,
            gShadowData.CascadeEnds[1].x,
            gShadowData.CascadeEnds[1].y,
            gShadowData.CascadeEnds[1].z,
            gShadowData.CascadeEnds[1].w,
        };

        int cascadeIndex = -1;
        for (int i = NUM_CASCADES - 1; i >= 0; i--)
            if (pin.PositionV.z < ends[i + 1])
                cascadeIndex = i;
    
        float shadowFactor = 1.0f;
        if (cascadeIndex != -1)
        {
            shadowFactor = PCSS(gShadowMap, cascadeIndex, gsamShadow, pointSampler, pin.PositionV, gShadowData, gInvView);
        }
        
        float3 directLighting = float3(0, 0, 0);
    
        [loop] // Unrolling produces strange behaviour so adding the [loop] attribute.
        for (int i = 0; i < 3; i++)
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
        
        //ImageAtomicRGBA8Avg(gVoxelizerAlbedo, texIndex, float4(directLighting / VOXEL_COMPRESS_COLOR_RANGE, alpha));
        //InterlockedAdd(gVoxelizerAlbedo[texIndex], convVec4ToRGBA8(float4(directLighting / VOXEL_COMPRESS_COLOR_RANGE * 255.0, alpha)));
        InterlockedAdd(gVoxelizerAlbedo[texIndex], convVec4ToRGBA8(float4(directLighting * 255.0 / VOXEL_COMPRESS_COLOR_RANGE, alpha)));
    }
}