#include "structs.hlsl"
#include "constants.hlsl"
#include "voxelUtils.hlsl"

struct Resources
{
    uint VoxelIndex;
};

ConstantBuffer<Resources> g_Resources : register(b6);

[numthreads(8,8,8)]
void main(uint3 ThreadID : SV_DispatchThreadID)
{
    RWStructuredBuffer<Voxel> voxels = ResourceDescriptorHeap[g_Resources.VoxelIndex];
    voxels[Flatten(ThreadID, VOXEL_DIMENSION)].Radiance = 0;
    voxels[Flatten(ThreadID, VOXEL_DIMENSION)].Normal = 0;
}