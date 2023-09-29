#include "utils.hlsl"

static const uint NumSamples = 1024;
static const float InvNumSamples = 1.0 / float(NumSamples);

cbuffer SpecularMapFilterSettings : register(b0)
{
	// Roughness value to pre-filter for.
	float roughness;
};

TextureCube inputTexture : register(t0);
RWTexture2DArray<float4> outputTexture : register(u0);

SamplerState defaultSampler : register(s0);

[numthreads(32, 32, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	uint outputWidth, outputHeight, outputDepth;
	outputTexture.GetDimensions(outputWidth, outputHeight, outputDepth);
	
	// Make sure we won't write past output when computing higher mipmap levels.
	if(threadID.x >= outputWidth || threadID.y >= outputHeight) 
		return;
	
	// Get input cubemap dimensions at zero mipmap level.
	float inputWidth, inputHeight, inputLevels;
	inputTexture.GetDimensions(0, inputWidth, inputHeight, inputLevels);
	
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
		float3 H = TangentToBasis(SampleGGX(uv.x, uv.y, roughness), N, S, T);

		// Compute incident direction (Li) by reflecting viewing direction (Lo) around half-vector (H).
		float3 L = 2.0 * dot(V, H) * H - V;

		float NdotL = dot(N, L);
		if(NdotL > 0.0) {
			// Use Mipmap Filtered Importance Sampling to improve convergence.
            // See: https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch20.html, section 20.4
            float NdotH = max(dot(N, H), 0.0);

            // GGX normal distribution function (D term) probability density function.
            // Scaling by 1/4 is due to change of density in terms of Lh to Li (and since N=V, rest of the scaling factor cancels out).
            float pdf = NDFGGX(NdotH, roughness) * 0.25;

            // Solid angle associated with this sample.
            float ws = 1.0 / (NumSamples * pdf);

            // Mip level to sample from.
            float mipLevel = max(0.5 * log2(ws / wt) + 1.0, 0.0);

            color += inputTexture.SampleLevel(defaultSampler, L, mipLevel).rgb * NdotL;
            weight += NdotL;
        }
	}
	color /= weight;

	outputTexture[threadID] = float4(color, 1.0);
}
