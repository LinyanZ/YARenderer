#ifndef __UTILS_HLSL__
#define __UTILS_HLSL__

#include "common.hlsl"

// Compute Van der Corput radical inverse
// See: http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

// Sample i-th point from Hammersley point set of NumSamples points total.
float2 SampleHammersley(uint i, float invNumSamples)
{
    return float2(i * invNumSamples, RadicalInverse_VdC(i));
}

// Uniformly sample point on a hemisphere.
// Cosine-weighted sampling would be a better fit for Lambertian BRDF but since this
// compute shader runs only once as a pre-processing step performance is not *that* important.
// See: "Physically Based Rendering" 2nd ed., section 13.6.1.
float3 SampleHemisphere(float u, float v)
{
    float phi = v * TwoPI;
    float cosTheta = 1.0 - u;
    float sinTheta = sqrt(max(0.0, 1.0 - cosTheta * cosTheta));
    return float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

// Importance sample GGX normal distribution function for a fixed roughness value.
// This returns normalized half-vector between Li & Lo.
// For derivation see: http://blog.tobias-franke.eu/2014/03/30/notes_on_importance_sampling.html
float3 SampleGGX(float u1, float u2, float roughness)
{
    float alpha = roughness * roughness;

    float cosTheta = sqrt((1.0 - u2) / (1.0 + (alpha * alpha - 1.0) * u2));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta); // Trig. identity
    float phi = TwoPI * u1;

	// Convert to Cartesian upon return.
    return float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

// Single term for separable Schlick-GGX below.
float GASchlickG1(float cosTheta, float k)
{
    return cosTheta / (cosTheta * (1.0 - k) + k);
}

// Schlick-GGX approximation of geometric attenuation function using Smith's method.
float GASchlickGGX(float cosLi, float cosLo, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0; // Epic suggests using this roughness remapping for analytic lights.
    return GASchlickG1(cosLi, k) * GASchlickG1(cosLo, k);
}

// Schlick-GGX approximation of geometric attenuation function using Smith's method (IBL version).
float GASchlickGGX_IBL(float cosLi, float cosLo, float roughness)
{
    float r = roughness;
    float k = (r * r) / 2.0; // Epic suggests using this roughness remapping for IBL lighting.
    return GASchlickG1(cosLi, k) * GASchlickG1(cosLo, k);
}

// Shlick's approximation of the Fresnel factor.
float3 FresnelSchlick(float3 F0, float cosTheta)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

// GGX/Towbridge-Reitz normal distribution function.
// Uses Disney's reparametrization of alpha = roughness^2.
float NDFGGX(float cosLh, float roughness)
{
    float alpha = roughness * roughness;
    float alphaSq = alpha * alpha;

    float denom = (cosLh * cosLh) * (alphaSq - 1.0) + 1.0;
    return alphaSq / (PI * denom * denom);
}

// Calculate normalized sampling direction vector based on current fragment coordinates.
// This is essentially "inverse-sampling": we reconstruct what the sampling vector would be if we wanted it to "hit"
// this particular fragment in a cubemap.
float3 GetSamplingVector(uint3 threadID, float width, float height)
{
    float2 st = threadID.xy / float2(width, height);
    float2 uv = 2.0 * float2(st.x, 1.0 - st.y) - 1.0;

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

// Compute orthonormal basis for converting from tanget/shading space to world space.
void ComputeBasisVectors(const float3 N, out float3 S, out float3 T)
{
	// Branchless select non-degenerate T.
    T = cross(N, float3(0.0, 1.0, 0.0));
    T = lerp(cross(N, float3(1.0, 0.0, 0.0)), T, step(Epsilon, dot(T, T)));

    T = normalize(T);
    S = normalize(cross(N, T));
}

// Convert point from tangent/shading space to world space.
float3 TangentToBasis(const float3 v, const float3 N, const float3 S, const float3 T)
{
    return S * v.x + T * v.y + N * v.z;
}

#endif // __UTILS_HLSL__