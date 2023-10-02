#include "structs.hlsl"
#include "common.hlsl"
#include "renderResources.hlsl"

ConstantBuffer<VoxelRenderResources> g_Resources : register(b6);

uint Flatten(uint3 texCoord)
{
    return texCoord.x * VOXEL_DIMENSION * VOXEL_DIMENSION + 
           texCoord.y * VOXEL_DIMENSION + 
           texCoord.z;
}

[numthreads(8,8,8)]
void main(uint3 ThreadID : SV_DispatchThreadID)
{
    RWStructuredBuffer<Voxel> voxels = ResourceDescriptorHeap[g_Resources.VoxelIndex];
    voxels[Flatten(ThreadID)].Radiance = 0;
}