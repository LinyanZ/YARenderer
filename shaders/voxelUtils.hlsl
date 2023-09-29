#include "common.hlsl"
#include "utils.hlsl"

#define VOXEL_OFFSET_CORRECTION_FACTOR 5
#define NUM_STEPS 30

uint3 WorldPosToVoxelIndex(float3 position)
{
    position = position / VOXEL_DIMENSION / VOXEL_GRID_SIZE;
    
    return uint3(
        (position.x * 0.5 + 0.5f) * VOXEL_DIMENSION,
        (position.y * -0.5 + 0.5f) * VOXEL_DIMENSION,
        (position.z * 0.5 + 0.5f) * VOXEL_DIMENSION
    );
}

// ref: https://andrew-pham.blog/2019/07/29/voxel-cone-tracing/
float4 TraceCone(Texture3D<float4> voxels, SamplerState linearSampler, float3 pos, float3 N, float3 direction, float aperture)
{
    float4 color = 0.0;
    float3 start = pos + VOXEL_OFFSET_CORRECTION_FACTOR * VOXEL_GRID_SIZE * N;
 
    float tanHalfAperture = tan(aperture / 2.0f);
    float tanEighthAperture = tan(aperture / 8.0f);
    float stepSizeCorrectionFactor = (1.0f + tanEighthAperture) / (1.0f - tanEighthAperture);
    float step = stepSizeCorrectionFactor * VOXEL_GRID_SIZE / 2.0f;
 
    float distance = step;
 
    for (int i = 0; i < NUM_STEPS && color.a <= 1.0f; ++i)
    {
        float3 position = start + distance * direction;
        uint3 texIndex = WorldPosToVoxelIndex(position);
        float3 texCoord = (float3) texIndex / VOXEL_DIMENSION;
        
        if (all(texCoord <= 1) && all(texCoord >= 0))
        {
            float diameter = 2.0f * tanHalfAperture * distance;
            float mipLevel = log2(diameter / VOXEL_GRID_SIZE);
 
            float4 voxelColor = voxels.SampleLevel(linearSampler, texCoord, mipLevel);
            if (voxelColor.a >= 0)
            {
                // Alpha blending
                float a = 1.0 - color.a;
                color.rgb += a * voxelColor.rgb;
                color.a += a * voxelColor.a;
            }
            distance += step;
        }
    }
    
    return color;
}

float4 TraceDiffuseCone(Texture3D<float4> voxels, SamplerState linearSampler, float3 pos, float3 N)
{
    float3 S, T;
    ComputeBasisVectors(N, S, T);
    
    float aperture = PI / 3.0f;
    float3 direction = N;
    
    float4 color = TraceCone(voxels, linearSampler, pos, N, direction, aperture);
    
    direction = 0.7071f * N + 0.7071f * T;
    color += TraceCone(voxels, linearSampler, pos, N, direction, aperture);
    // Rotate the tangent vector about the normal using the 5th roots of unity to obtain the subsequent diffuse cone directions
    direction = 0.7071f * N + 0.7071f * (0.309f * T + 0.951f * S);
    color += TraceCone(voxels, linearSampler, pos, N, direction, aperture);
    direction = 0.7071f * N + 0.7071f * (-0.809f * T + 0.588f * S);
    color += TraceCone(voxels, linearSampler, pos, N, direction, aperture);
    direction = 0.7071f * N - 0.7071f * (-0.809f * T - 0.588f * S);
    color += TraceCone(voxels, linearSampler, pos, N, direction, aperture);
    direction = 0.7071f * N - 0.7071f * (0.309f * T - 0.951f * S);
    color += TraceCone(voxels, linearSampler, pos, N, direction, aperture);
 
    return color / 6.0f;
}

float4 TraceSpecularCone(Texture3D<float4> voxels, SamplerState linearSampler, float3 pos, float3 N, float3 V, float roughness)
{
    float aperture = roughness;
    float3 direction = reflect(-V, N);
    
    float4 color = TraceCone(voxels, linearSampler, pos, N, direction, aperture);
    color.rgb = max(0, color.rgb);
    color.a = saturate(color.a);
    
    return color;
}