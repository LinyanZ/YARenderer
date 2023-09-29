#include "common.hlsl"

RWTexture3D<uint> gVoxelizerRadiance : register(u0);

float4 convRGBA8ToVec4(uint value)
{
    return float4(float((value & 0x000000FF)), float((value & 0x0000FF00) >> 8U), float((value & 0x00FF0000) >> 16U), float((value & 0xFF000000) >> 24U));
}

uint convVec4ToRGBA8(float4 val)
{
    return (uint(val.w) & 0x000000FF) << 24U | (uint(val.z) & 0x000000FF) << 16U | (uint(val.y) & 0x000000FF) << 8U | (uint(val.x) & 0x000000FF);
}

[numthreads(8, 8, 8)]
void main(uint3 ThreadID : SV_DispatchThreadID)
{
    float4 color = convRGBA8ToVec4(gVoxelizerRadiance[ThreadID]);
    if (color.a <= 0)
    {
        gVoxelizerRadiance[ThreadID] = 0;
        return;
    }
    
    color.rgb /= 255.0;
    color.rgb *= VOXEL_COMPRESS_COLOR_RANGE;
    
    color /= color.a;
    
    gVoxelizerRadiance[ThreadID] = convVec4ToRGBA8(color * 255.0);
}
