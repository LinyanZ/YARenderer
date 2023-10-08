#include "samplers.hlsl"
#include "constants.hlsl"

struct Resources
{
    uint InputTexIndex;
    uint OutputTexIndex;
};

ConstantBuffer<Resources> g_Resources : register(b6);

Texture2D inputTexture : register(t0);
RWTexture2DArray<float4> outputTexture : register(u0);

SamplerState defaultSampler : register(s0);

// Calculate normalized sampling direction vector based on current fragment coordinates.
// This is essentially "inverse-sampling": we reconstruct what the sampling vector would be if we wanted it to "hit"
// this particular fragment in a cubemap.
float3 GetSamplingVector(uint3 threadID, float outputWidth, float outputHeight)
{
    float2 st = threadID.xy / float2(outputWidth, outputHeight);
    float2 uv = 2.0 * float2(st.x, 1.0 - st.y) - float2(1.0, 1.0);

	// Select vector based on cubemap face index.
    float3 ret;
    switch (threadID.z)
    {
        case 0:
            ret = float3(1.0, uv.y, -uv.x);
            break;
        case 1:
            ret = float3(-1.0, uv.y, uv.x);
            break;
        case 2:
            ret = float3(uv.x, 1.0, -uv.y);
            break;
        case 3:
            ret = float3(uv.x, -1.0, uv.y);
            break;
        case 4:
            ret = float3(uv.x, uv.y, 1.0);
            break;
        case 5:
            ret = float3(-uv.x, uv.y, -1.0);
            break;
    }
    return normalize(ret);
}

[numthreads(32, 32, 1)]
void main(uint3 threadID : SV_DispatchthreadID)
{
    Texture2D inputTex = ResourceDescriptorHeap[g_Resources.InputTexIndex];
    RWTexture2DArray<float4> outputTex = ResourceDescriptorHeap[g_Resources.OutputTexIndex];

    float outputWidth, outputHeight, outputDepth;
    outputTex.GetDimensions(outputWidth, outputHeight, outputDepth);

    float3 v = GetSamplingVector(threadID, outputWidth, outputHeight);
	
	// Convert Cartesian direction vector to spherical coordinates.
    float phi = atan2(v.z, v.x);
    float theta = acos(v.y);

	// Sample equirectangular texture.
    float4 color = inputTex.SampleLevel(g_SamplerAnisotropicWrap, float2(phi / TWO_PI, theta / PI), 0);

	// Write out color to output cubemap.
    outputTex[threadID] = color;
}