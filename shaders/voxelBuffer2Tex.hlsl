#include "structs.hlsl"
#include "common.hlsl"

struct Resources
{
    uint BufferIndex;
    uint TexIndex;
};

ConstantBuffer<Resources> g_Resources : register(b6);

float4 convRGBA8ToVec4(uint value)
{
    return float4(float((value & 0x000000FF)), float((value & 0x0000FF00) >> 8U), float((value & 0x00FF0000) >> 16U), float((value & 0xFF000000) >> 24U));
}

uint convVec4ToRGBA8(float4 val)
{
    return (uint(val.w) & 0x000000FF) << 24U | (uint(val.z) & 0x000000FF) << 16U | (uint(val.y) & 0x000000FF) << 8U | (uint(val.x) & 0x000000FF);
}

uint64_t ConvFloat4ToUINT64(float4 val)
{
    return (uint(val.w) & 0x0000FFFF) << 48U | (uint(val.z) & 0x0000FFFF) << 32U | (uint(val.y) & 0x0000FFFF) << 16U | (uint(val.x) & 0x0000FFFF);
}

float4 ConvUINT64ToFloat4(uint64_t val)
{
    float4 re = float4(float((val & 0x0000FFFF)), float((val & 0xFFFF0000) >> 16U), float((val & 0xFFFF00000000) >> 32U), float((val & 0xFFFF000000000000) >> 48U));
    return clamp(re, float4(0.0, 0.0, 0.0, 0.0), float4(65535.0, 65535.0, 65535.0, 65535.0));
}

uint Flatten(uint3 texCoord)
{
    return texCoord.x * VOXEL_DIMENSION * VOXEL_DIMENSION + 
           texCoord.y * VOXEL_DIMENSION + 
           texCoord.z;
}

[numthreads(8, 8, 8)]
void main(uint3 ThreadID : SV_DispatchThreadID)
{
    RWStructuredBuffer<Voxel> buffer = ResourceDescriptorHeap[g_Resources.BufferIndex];
    RWTexture3D<float4> tex = ResourceDescriptorHeap[g_Resources.TexIndex];

    float4 color = ConvUINT64ToFloat4(buffer[Flatten(ThreadID)].Radiance);
    if (color.a <= 0)
    {
        tex[ThreadID] = 0;
        return;
    }
    
    color.rgb /= 50.0;
    color.a /= 255.0;
    color /= color.a;
    tex[ThreadID] = color;

    // color.rgb /= 255.0;
    // color.rgb *= VOXEL_COMPRESS_COLOR_RANGE;
    
    // color /= color.a;
    
    // gVoxelizerRadiance[ThreadID] = convVec4ToRGBA8(color * 255.0);
}
