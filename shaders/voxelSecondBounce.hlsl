#include "structs.hlsl"
#include "constants.hlsl"
#include "voxelUtils.hlsl"
#include "samplers.hlsl"
#include "constantBuffers.hlsl"

struct Resources
{
    uint BufferIndex;
    uint InputTexIndex;
    uint OutputTexIndex;
};

ConstantBuffer<Resources> g_Resources : register(b6);

[numthreads(8, 8, 8)]
void main(uint3 texCoord : SV_DispatchThreadID)
{
    RWStructuredBuffer<Voxel> buffer = ResourceDescriptorHeap[g_Resources.BufferIndex];
    Texture3D<float4> input = ResourceDescriptorHeap[g_Resources.InputTexIndex];
    RWTexture3D<float4> output = ResourceDescriptorHeap[g_Resources.OutputTexIndex];

    uint bufferIndex = Flatten(texCoord, VOXEL_DIMENSION);

    if (buffer[bufferIndex].Normal == 0)
    {
        output[texCoord] = 0;
    }
    else
    {
        float3 uvw = (texCoord + 0.5) / VOXEL_DIMENSION;
        float3 voxelCenter = uvw * 2 - 1;
        voxelCenter.y *= -1;
        voxelCenter *= VOXEL_DIMENSION;
        voxelCenter *= VOXEL_GRID_SIZE;
        voxelCenter += VOXEL_GRID_WORLD_POS;

        float4 normals = ConvUINT64ToFloat4(buffer[bufferIndex].Normal);
        float3 normal = normals.xyz / 50.0;
        normal /= normals.w;
        normal = normalize(normal);

        float4 color = TraceDiffuseCone(input, g_SamplerLinearClamp, voxelCenter, normal);
        color.a = 1;

        color += input[texCoord];
        color /= color.a;

        output[texCoord] = color;
    }

    // reset buffer
    buffer[bufferIndex].Radiance = 0;
    buffer[bufferIndex].Normal = 0;
}
