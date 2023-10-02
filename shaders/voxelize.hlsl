#include "samplers.hlsl"
#include "constantBuffers.hlsl"
#include "structs.hlsl"
#include "common.hlsl"
#include "renderResources.hlsl"
#include "cascadedShadow.hlsl"

struct Resources
{
    uint VoxelIndex;
    uint ShadowMapTexIndex;
};

ConstantBuffer<Resources> g_Resources : register(b6);

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

struct VertexIn
{
    float3 Position     : POSITION;
    float3 Normal       : NORMAL;
    float3 Tangent      : TANGENT;
    float3 Bitangent    : BITANGENT;
    float2 TexCoord     : TEXCOORD0;
};

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
    
    // transform the position into world space as we will use 
    // this to index into the voxel grid and write to it
    vout.PositionW = mul(float4(vin.Position, 1.0f), g_World).xyz;
    vout.NormalW = mul(vin.Normal, (float3x3) g_World);
    
    float4x4 ModelView = mul(g_World, g_View);
    
    // transform to view space
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



uint64_t ConvFloat4ToUINT64(float4 val)
{
    return (uint64_t(val.w) & 0x0000FFFF) << 48U | (uint64_t(val.z) & 0x0000FFFF) << 32U | (uint64_t(val.y) & 0x0000FFFF) << 16U | (uint64_t(val.x) & 0x0000FFFF);
}

float4 ConvUINT64ToFloat4(uint64_t val)
{
    float4 re = float4(float((val & 0x0000FFFF)), float((val & 0xFFFF0000) >> 16U), float((val & 0xFFFF00000000) >> 32U), float((val & 0xFFFF000000000000) >> 48U));
    return clamp(re, float4(0.0, 0.0, 0.0, 0.0), float4(65535.0, 65535.0, 65535.0, 65535.0));
}

void ImageAtomicUINT64Avg(RWStructuredBuffer<Voxel> voxels, uint index, float4 val)
{
    val.rgb *= 65535.0f;
    uint64_t newVal = ConvFloat4ToUINT64(val);
    
    uint64_t prevStoredVal = 0;
    uint64_t curStoredVal;
    
    InterlockedCompareExchange(voxels[index].Radiance, prevStoredVal, newVal, curStoredVal);
    while (curStoredVal != prevStoredVal)
    {
        prevStoredVal = curStoredVal;
        float4 rval = ConvUINT64ToFloat4(curStoredVal);
        rval.xyz *= rval.w;
        
        float4 curValF = rval + val;
        curValF.xyz /= curValF.w;
        
        newVal = ConvFloat4ToUINT64(curValF);
        InterlockedCompareExchange(voxels[index].Radiance, prevStoredVal, newVal, curStoredVal);
    }
}

uint Flatten(uint3 texCoord)
{
    return texCoord.x * VOXEL_DIMENSION * VOXEL_DIMENSION + 
           texCoord.y * VOXEL_DIMENSION + 
           texCoord.z;
}

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

void PS(GeometryInOut pin)
{
    RWStructuredBuffer<Voxel> voxels = ResourceDescriptorHeap[g_Resources.VoxelIndex];

    float4 albedoCol = GetAlbedo(pin.TexCoord);
    float3 albedo = albedoCol.rgb;
    float alpha = albedoCol.a;

    float3 normal = GetNormal(pin.NormalV, pin.TangentV, pin.BitangentV, pin.TexCoord);
    float metalness = GetMetalness(pin.TexCoord);
    float roughness = GetRoughness(pin.TexCoord);
    
    uint3 texIndex = uint3(
        (pin.PositionW.x * 0.5 + 0.5f) * VOXEL_DIMENSION,
        (pin.PositionW.y * -0.5 + 0.5f) * VOXEL_DIMENSION,
        (pin.PositionW.z * 0.5 + 0.5f) * VOXEL_DIMENSION
    );    
    
    if (all(texIndex < VOXEL_DIMENSION) && all(texIndex >= 0))
    {
        float ends[] = {
            g_ShadowData.CascadeEnds[0].x,
            g_ShadowData.CascadeEnds[0].y,
            g_ShadowData.CascadeEnds[0].z,
            g_ShadowData.CascadeEnds[0].w,
            g_ShadowData.CascadeEnds[1].x,
            g_ShadowData.CascadeEnds[1].y,
            g_ShadowData.CascadeEnds[1].z,
            g_ShadowData.CascadeEnds[1].w,
        };

        int cascadeIndex = -1, i;
        for (i = NUM_CASCADES - 1; i >= 0; i--)
            if (pin.PositionV.z < ends[i + 1])
                cascadeIndex = i;
    
        float shadowFactor = 1.0f;
        if (cascadeIndex != -1)
        {
            Texture2DArray shadowMap = ResourceDescriptorHeap[g_Resources.ShadowMapTexIndex];

            float4 positionW = mul(float4(pin.PositionV, 1.0f), g_InvView);
            float4 positionH = mul(positionW, g_ShadowData.LightViewProj[cascadeIndex]);
            positionH.xyz /= positionH.w;
            
            float depth = positionH.z;
            
            // NDC space to texture space
            // [-1, 1] -> [0, 1] and flip the y-coord
            float2 shadowMapUV = (positionH.xy + 1.0f) * 0.5f;
            shadowMapUV.y = 1.0f - shadowMapUV.y;

            shadowFactor = shadowMap.SampleCmpLevelZero(g_SamplerShadow, float3(shadowMapUV, cascadeIndex), depth).r;
        }
        
        float3 directLighting = float3(0, 0, 0);
    
        [loop] // Unrolling produces strange behaviour so adding the [loop] attribute.
        for (i = 0; i < 1; i++)
        {
            // Skip lights that are not enabled.
            if (!LightCB[i].Enabled)
                continue;
        
            // Skip point and spot lights that are out of range of the point being shaded.
            if (LightCB[i].Type != DIRECTIONAL_LIGHT &&
                 length(LightCB[i].PositionVS.xyz - pin.PositionV) > LightCB[i].Range)
                continue;
 
            switch (LightCB[i].Type)
            {
                case DIRECTIONAL_LIGHT:
                    directLighting += DoDirectionalLight(LightCB[i], albedo, normal, metalness, roughness, pin.PositionV) * shadowFactor;
                    break;
                case POINT_LIGHT:
                    directLighting += DoPointLight(LightCB[i], albedo, normal, metalness, roughness, pin.PositionV);
                    break;
                case SPOT_LIGHT:
                    directLighting += DoSpotLight(LightCB[i], albedo, normal, metalness, roughness, pin.PositionV);
                    break;
                default:
                    break;
            }
        }
        
        InterlockedAdd(voxels[Flatten(texIndex)].Radiance, ConvFloat4ToUINT64(float4(directLighting * 50.0, alpha * 255.0)));
        // ImageAtomicUINT64Avg(voxels, Flatten(texIndex), float4(directLighting, alpha));
    }
}