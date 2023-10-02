struct Resources
{
    uint PrevMipIndex;
    uint CurrMipIndex;
};

ConstantBuffer<Resources> g_Resources : register(b6);

[numthreads(8, 8, 8)]
void main(uint3 ThreadID : SV_DispatchThreadID)
{
    float4 gatherValue = 0.0;
    RWTexture3D<float4> prevMip = ResourceDescriptorHeap[g_Resources.PrevMipIndex];
    RWTexture3D<float4> currMip = ResourceDescriptorHeap[g_Resources.CurrMipIndex];
    
    [unroll]
    for (int i = 0; i < 2; i++)
    {
        [unroll]
        for (int j = 0; j < 2; j++)
        {
            [unroll]
            for (int k = 0; k < 2; k++)
            {
                gatherValue += prevMip[2 * ThreadID + uint3(i, j, k)];
            }
        }
    }
    
    gatherValue *= 0.125;
    currMip[ThreadID] = gatherValue;
}
