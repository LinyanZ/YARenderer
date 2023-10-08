#include "utils.hlsl"
#include "samplers.hlsl"

static const uint NumSamples = 1024;
static const float InvNumSamples = 1.0 / float(NumSamples);

struct Resources
{
    uint InputTexIndex;
    uint OutputTexIndex;
	float Roughness;
};

ConstantBuffer<Resources> g_Resources : register(b6);

[numthreads(32, 32, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	TextureCube inputTex = ResourceDescriptorHeap[g_Resources.InputTexIndex];
    RWTexture2DArray<float4> outputTex = ResourceDescriptorHeap[g_Resources.OutputTexIndex];

	uint outputWidth, outputHeight, outputDepth;
	outputTex.GetDimensions(outputWidth, outputHeight, outputDepth);
	
	// Make sure we won't write past output when computing higher mipmap levels.
	if(threadID.x >= outputWidth || threadID.y >= outputHeight) 
		return;
	
	// Get input cubemap dimensions at zero mipmap level.
	float inputWidth, inputHeight, inputLevels;
	inputTex.GetDimensions(0, inputWidth, inputHeight, inputLevels);
	
	// Solid angle associated with a single cubemap texel at zero mipmap level.
    // This will come in handy for importance sampling below.
    float wt = 4.0 * PI / (6 * inputWidth * inputHeight);
	
	// Approximation: Assume zero viewing angle (isotropic reflections).
	float3 N = GetSamplingVector(threadID, outputWidth, outputHeight);
	float3 V = N;
	
	float3 S, T;
	ComputeBasisVectors(N, S, T);

	float3 color = 0;
	float weight = 0;

	// Convolve environment map using GGX NDF importance sampling.
	// Weight by cosine term since Epic claims it generally improves quality.
	for(uint i = 0; i < NumSamples; ++i) {
		float2 uv = SampleHammersley(i, InvNumSamples);
		float3 H = TangentToBasis(SampleGGX(uv.x, uv.y, g_Resources.Roughness), N, S, T);

		// Compute incident direction (L) by reflecting viewing direction (V) around half-vector (H).
		float3 L = 2.0 * dot(V, H) * H - V;

		float NdotL = dot(N, L);
		if(NdotL > 0.0) {
			// Use Mipmap Filtered Importance Sampling to improve convergence.
            // See: https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch20.html, section 20.4
            float NdotH = max(dot(N, H), 0.0);

            // GGX normal distribution function (D term) probability density function.
            // Scaling by 1/4 is due to change of density in terms of Lh to Li (and since N=V, rest of the scaling factor cancels out).
            float pdf = NDFGGX(NdotH, g_Resources.Roughness) * 0.25;

            // Solid angle associated with this sample.
            float ws = 1.0 / (NumSamples * pdf);

            // Mip level to sample from.
            float mipLevel = max(0.5 * log2(ws / wt) + 1.0, 0.0);

            color += inputTex.SampleLevel(g_SamplerAnisotropicWrap, L, mipLevel).rgb * NdotL;
            weight += NdotL;
        }
	}
	color /= weight;

	outputTex[threadID] = float4(color, 1.0);
}
