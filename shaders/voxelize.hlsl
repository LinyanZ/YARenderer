#include "constants.hlsl"
#include "structs.hlsl"
#include "constantBuffers.hlsl"
#include "samplers.hlsl"
#include "common.hlsl"
#include "cascadedShadow.hlsl"
#include "voxelUtils.hlsl"

struct Resources
{
    uint VoxelIndex;
    uint ShadowMapTexIndex;
};

ConstantBuffer<Resources> g_Resources : register(b6);

float3 DoDirectLighting(Light light, float3 albedo, float3 normal, float metalness, float roughness, float3 L, float3 V)
{
    float NdotL = max(0.0, dot(normal, L));
    float3 kd = lerp(float3(1, 1, 1), float3(0, 0, 0), metalness);

    // Lambert diffuse BRDF.
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
        
        output[i].PositionW = input[i].PositionW;
        output[i].PositionV = input[i].PositionV;
        output[i].NormalW = input[i].NormalW;
        output[i].NormalV = input[i].NormalV;
        output[i].TangentV = input[i].TangentV;
        output[i].BitangentV = input[i].BitangentV;
        output[i].TexCoord = input[i].TexCoord;
        
        triStream.Append(output[i]);
    }
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
    
    float3 positionW = pin.PositionW / VOXEL_DIMENSION / VOXEL_GRID_SIZE;
    uint3 texIndex = uint3(
        (positionW.x * 0.5 + 0.5f) * VOXEL_DIMENSION,
        (positionW.y * -0.5 + 0.5f) * VOXEL_DIMENSION,
        (positionW.z * 0.5 + 0.5f) * VOXEL_DIMENSION
    );    
    
    if (all(texIndex < VOXEL_DIMENSION) && all(texIndex >= 0))
    {
        float shadowFactor = 1.0f;

        int cascadeIndex = GetCascadeIndex(pin.PositionV);
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
        for (int i = 0; i < 1; i++)
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
        
        uint voxelIndex = Flatten(texIndex, VOXEL_DIMENSION);
        InterlockedAdd(voxels[voxelIndex].Radiance, ConvFloat4ToUINT64(float4(directLighting * 50.0, alpha * 255.0)));
        InterlockedAdd(voxels[voxelIndex].Normal, ConvFloat4ToUINT64(float4(pin.NormalW * 50.0, 1)));
        // ImageAtomicUINT64Avg(voxels, Flatten(texIndex), float4(directLighting, alpha));
    }
}