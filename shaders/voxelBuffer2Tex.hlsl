#include "structs.hlsl"
#include "constants.hlsl"
#include "voxelUtils.hlsl"

struct Resources
{
    uint BufferIndex;
    uint TexIndex;
};

ConstantBuffer<Resources> g_Resources : register(b6);

[numthreads(8, 8, 8)]
void main(uint3 texCoord : SV_DispatchThreadID)
{
    RWStructuredBuffer<Voxel> buffer = ResourceDescriptorHeap[g_Resources.BufferIndex];
    RWTexture3D<float4> tex = ResourceDescriptorHeap[g_Resources.TexIndex];

    uint bufferIndex = Flatten(texCoord, VOXEL_DIMENSION);
    float4 color = ConvUINT64ToFloat4(buffer[bufferIndex].Radiance);
    
    // reset buffer
    // buffer[bufferIndex].Radiance = 0;

    if (color.a <= 0)
    {
        tex[texCoord] = 0;
        return;
    }
    
    color.rgb /= 50.0;
    color.a /= 255.0;

    // avoid very bright spot when alpha is low
    color.a = max(color.a, 1); 
    color /= color.a;

    tex[texCoord] = color;

    // color.rgb /= 255.0;
    // color.rgb *= VOXEL_COMPRESS_COLOR_RANGE;
    
    // color /= color.a;
    
    // gVoxelizerRadiance[ThreadID] = convVec4ToRGBA8(color * 255.0);
}
