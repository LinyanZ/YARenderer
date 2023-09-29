RWTexture3D<uint> gVoxelizerRadiance : register(u0);
RWTexture3D<uint> gVoxelizerMipedRadiance : register(u1);

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
    float4 gatherValue = 0.0;
    
    [unroll]
    for (int i = 0; i < 2; i++)
    {
        [unroll]
        for (int j = 0; j < 2; j++)
        {
            [unroll]
            for (int k = 0; k < 2; k++)
            {
                float4 color = convRGBA8ToVec4(gVoxelizerRadiance[2 * ThreadID + uint3(i, j, k)]) / 255.0;
                gatherValue += color;
            }
        }
    }
    
    gatherValue *= 0.125;
    gVoxelizerMipedRadiance[ThreadID] = convVec4ToRGBA8(gatherValue * 255.0);
}
