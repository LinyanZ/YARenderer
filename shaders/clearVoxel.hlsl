RWTexture3D<uint> volumeTexture : register(u0);

[numthreads(8,8,8)]
void main(uint3 ThreadID : SV_DispatchThreadID)
{
    volumeTexture[ThreadID] = 0;
}