// Physically Based Rendering
// Copyright (c) 2017-2018 Michał Siejak

// Texture mip level downsampling with linear filtering (used in manual mip chain generation).

struct Resources
{
    uint InputTexIndex;
    uint OutputTexIndex;
};

ConstantBuffer<Resources> g_Resources : register(b6);

[numthreads(8, 8, 1)]
void downsample_linear(uint3 ThreadID : SV_DispatchThreadID)
{
	Texture2DArray intputTex = ResourceDescriptorHeap[g_Resources.InputTexIndex];
	RWTexture2DArray<float4> outputTex = ResourceDescriptorHeap[g_Resources.OutputTexIndex];

	int4 sampleLocation = int4(2 * ThreadID.x, 2 * ThreadID.y, ThreadID.z, 0);
	float4 gatherValue = 
		intputTex.Load(sampleLocation, int2(0, 0)) +
		intputTex.Load(sampleLocation, int2(1, 0)) +
		intputTex.Load(sampleLocation, int2(0, 1)) +
		intputTex.Load(sampleLocation, int2(1, 1));
	outputTex[ThreadID] = 0.25 * gatherValue;
}
