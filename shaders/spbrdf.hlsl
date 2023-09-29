#include "utils.hlsl"

static const uint NumSamples = 1024;
static const float InvNumSamples = 1.0 / float(NumSamples);

RWTexture2D<float2> LUT : register(u0);

[numthreads(32, 32, 1)]
void main(uint2 ThreadID : SV_DispatchThreadID)
{
	// Get output LUT dimensions.
	float outputWidth, outputHeight;
	LUT.GetDimensions(outputWidth, outputHeight);

	// Get integration parameters.
	float NdotV = ThreadID.x / outputWidth;
	float roughness = ThreadID.y / outputHeight;

	// Make sure viewing angle is non-zero to avoid divisions by zero (and subsequently NaNs).
    NdotV = max(NdotV, Epsilon);

	// Derive tangent-space viewing vector from angle to normal (pointing towards +Z in this reference frame).
    float3 V = float3(sqrt(1.0 - NdotV * NdotV), 0.0, NdotV);

	// We will now pre-integrate Cook-Torrance BRDF for a solid white environment and save results into a 2D LUT.
	// DFG1 & DFG2 are terms of split-sum approximation of the reflectance integral.
	// For derivation see: "Moving Frostbite to Physically Based Rendering 3.0", SIGGRAPH 2014, section 4.9.2.
	float DFG1 = 0;
	float DFG2 = 0;

	for(uint i=0; i<NumSamples; ++i) {
		float2 uv = SampleHammersley(i, InvNumSamples);

		// Sample directly in tangent/shading space since we don't care about reference frame as long as it's consistent.
		float3 H = SampleGGX(uv.x, uv.y, roughness);

		// Compute incident direction (Li) by reflecting viewing direction (Lo) around half-vector (Lh).
		float3 L = 2.0 * dot(V, H) * H - V;

		float NdotL = L.z;
		float NdotH = H.z;
		float VdotH = max(dot(V, H), 0.0);

        if (NdotL > 0.0)
        {
            float G = GASchlickGGX_IBL(NdotL, NdotV, roughness);
            float Gv = G * VdotH / (NdotH * NdotV);
			float Fc = pow(1.0 - VdotH, 5);

			DFG1 += (1 - Fc) * Gv;
			DFG2 += Fc * Gv;
		}
	}

	LUT[ThreadID] = float2(DFG1, DFG2) * InvNumSamples;
}
