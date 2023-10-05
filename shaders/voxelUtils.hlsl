#include "constants.hlsl"
#include "utils.hlsl"

#define VOXEL_OFFSET_CORRECTION_FACTOR 5
#define NUM_STEPS 100

uint Flatten(uint3 texCoord, uint dim)
{
    return texCoord.x * dim * dim + 
           texCoord.y * dim + 
           texCoord.z;
}

uint3 Unflatten(uint i, uint dim)
{
    uint3 coord;
    
    coord[0] = i % dim;
    i /= dim;
    
    coord[1] = i % dim;
    i /= dim;
    
    coord[2] = i % dim;
    
    return coord;
}

// ref: https://xeolabs.com/pdfs/OpenGLInsights.pdf Chapter 22
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

uint3 WorldPosToVoxelIndex(float3 position)
{
    position = position / VOXEL_DIMENSION / VOXEL_GRID_SIZE;
    
    return uint3(
        (position.x * 0.5 + 0.5f) * VOXEL_DIMENSION,
        (position.y * -0.5 + 0.5f) * VOXEL_DIMENSION,
        (position.z * 0.5 + 0.5f) * VOXEL_DIMENSION
    );
}

float4 TraceCone(Texture3D<float4> voxels, SamplerState linearSampler, float3 pos, float3 N, float3 direction, float aperture)
{
    float4 color = 0.0;
 
    float tanHalfAperture = tan(aperture / 2.0f);
    float tanEighthAperture = tan(aperture / 8.0f);

    // taken from https://andrew-pham.blog/2019/07/29/voxel-cone-tracing/
    // don't really understand why but the result looks good
    // basically the step size increases when aperture gets bigger
    float stepSizeCorrectionFactor = (1.0f + tanEighthAperture) / (1.0f - tanEighthAperture);
    float step = stepSizeCorrectionFactor * VOXEL_GRID_SIZE / 2.0f;
    float distance = step;
    
    // full extent of a voxel
    float offset = VOXEL_GRID_SIZE * sqrt(3); 
    
    // offset along the normal direction to avoid sampling itself
    float3 start = pos + offset * N;  
 
    // ray-marching along the direction
    // color.a acts as occlusion, we stop when we reaches one
    for (int i = 0; i < NUM_STEPS && color.a < 1.0f; ++i)
    {
        float3 position = start + distance * direction;
        uint3 texIndex = WorldPosToVoxelIndex(position);
        float3 texCoord = (float3) texIndex / VOXEL_DIMENSION;
        
        // break if outside the range
        if (any(texCoord < 0) || any(texCoord) > 1)
            break;

        // determine which mip level to sample from
        float diameter = 2.0f * tanHalfAperture * distance;
        float mipLevel = log2(diameter / VOXEL_GRID_SIZE);

        float4 voxelColor = voxels.SampleLevel(linearSampler, texCoord, mipLevel);
        if (voxelColor.a > 0)
        {
            // front-to-back alpha blending
            float a = 1.0 - color.a;
            color.rgb += a * voxelColor.rgb;
            color.a += a * voxelColor.a;
        }

        distance += step;
    }
    
    return color;
}

float4 TraceDiffuseCone(Texture3D<float4> voxels, SamplerState linearSampler, float3 pos, float3 N)
{
    float3 S, T;
    ComputeBasisVectors(N, S, T);
    
    // angle of the cone
    float aperture = PI / 3.0f;

    float3 direction = N;
    float4 color = TraceCone(voxels, linearSampler, pos, N, direction, aperture);
    
    // rotate 45 deg towards T from N
    // sqrt(2) / 2 = 0.7071
    direction = 0.7071f * N + 0.7071f * T;
    color += TraceCone(voxels, linearSampler, pos, N, direction, aperture);

    // rotate the tangent vector about the normal with a step size of 72 deg (72 * 5 = 360)
    // to obtain the subsequent diffuse cone directions

    // cos(72) = 0.309, sin(72) = 0.951
    direction = 0.7071f * N + 0.7071f * (0.309f * T + 0.951f * S);
    color += TraceCone(voxels, linearSampler, pos, N, direction, aperture);

    // cos(144) = -0.809, sin(144) = 0.588
    direction = 0.7071f * N + 0.7071f * (-0.809f * T + 0.588f * S);
    color += TraceCone(voxels, linearSampler, pos, N, direction, aperture);

    // cos(216), sin(216)
    direction = 0.7071f * N - 0.7071f * (-0.809f * T - 0.588f * S);
    color += TraceCone(voxels, linearSampler, pos, N, direction, aperture);

    // cos(288), sin(288)
    direction = 0.7071f * N - 0.7071f * (0.309f * T - 0.951f * S);
    color += TraceCone(voxels, linearSampler, pos, N, direction, aperture);
 
    color /= 6.0f;
    color.rgb = max(0, color.rgb);
    color.a = saturate(color.a);
    
    return color;
}

float4 TraceSpecularCone(Texture3D<float4> voxels, SamplerState linearSampler, float3 pos, float3 N, float3 V, float roughness)
{
    float aperture = 0.03;
    float3 direction = reflect(-V, N);
    
    float4 color = TraceCone(voxels, linearSampler, pos, N, direction, aperture);
    color.rgb = max(0, color.rgb);
    color.a = saturate(color.a);
    
    return color;
}