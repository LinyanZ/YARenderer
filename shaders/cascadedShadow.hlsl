#include "common.hlsl"
#include "structs.hlsl"
#include "samplers.hlsl"

#define SHADOW_MAP_SIZE 4096
#define NUM_CASCADES 4
#define MAX_SAMPLES 64
#define LIGHT_SIZE 0.04
#define MIN_FILTER_RADIUS (2.0 / SHADOW_MAP_SIZE)
#define NUM_RINGS 10

static float2 g_SampleOffsets[MAX_SAMPLES];

float Random(float2 seed)
{
    return frac(sin(dot(seed, float2(12.9898, 78.233))) * 43758.5453);
}

// taken from https://www.gamedev.net/tutorials/programming/graphics/contact-hardening-soft-shadows-made-fast-r4906/
void VogelDiskSamples(float2 seed, int sampleCount)
{
    float phi = Random(seed) * TwoPI;
    float GoldenAngle = 2.4f;
    
    for (int i = 0; i < sampleCount; i++)
    {
        float r = sqrt(i + 0.5f) / sqrt(sampleCount);
        float theta = i * GoldenAngle + phi;
        
        float sine, cosine;
        sincos(theta, sine, cosine);

        g_SampleOffsets[i] = float2(r * cosine, r * sine);
    }
}

// taken from Games202 homework framework: https://sites.cs.ucsb.edu/~lingqi/teaching/games202.html
void PoissonDiskSamples(float2 seed, int sampleCount)
{
    float angleStep = TwoPI * float(NUM_RINGS) / float(sampleCount);
    float invNumSamples = 1.0 / float(sampleCount);
    
    float angle = Random(seed) * TwoPI;
    float radius = invNumSamples;
    float radiusStep = radius;
    
    for (int i = 0; i < sampleCount; i++)
    {
        g_SampleOffsets[i] = float2(cos(angle), sin(angle)) * pow(radius, 0.5);
        radius += radiusStep;
        angle += angleStep;
    }
}

float FindBlocker(Texture2DArray cascadeShadowMap, int cascadeIndex, float2 uv, float depth)
{
    float firstCascadeRadius = ((float[4]) g_ShadowData.CascadeRadius)[0];
    float currentCascadeRadius = ((float[4]) g_ShadowData.CascadeRadius)[cascadeIndex];
    float cascadeRatio = firstCascadeRadius / currentCascadeRadius;
    
    // It appears that the shadow implementation in Nvidia's whitepaper is using perspective projection.
    // The original formula is (depth - NEAR_PLANE) / depth * g_ShadowData.ShadowSoftness * LIGHT_SIZE * cascadeRatio.
    // However, I'm using orthogonal projection for shadow mapping, where the size of the light remains consistent regardless of the near plane value.
    // Hence the formula is simplified.
    float searchWidth = g_ShadowData.ShadowSoftness * LIGHT_SIZE * cascadeRatio;
    float blockerSum = 0;
    int numBlockers = 0;
 
    for (int i = 0; i < g_ShadowData.NumSamples; ++i)
    {
        float2 offset = g_SampleOffsets[i] * searchWidth;
        float shadowMapDepth = cascadeShadowMap.SampleLevel(g_SamplerPointClamp, float3(uv + offset, cascadeIndex), 0).r;
        if (shadowMapDepth < depth)
        {
            blockerSum += shadowMapDepth;
            numBlockers++;
        }
    }
    
    if (numBlockers == 0)
        return -1.0;
    
    return blockerSum / numBlockers;
}

float PCF(Texture2DArray cascadeShadowMap, int cascadeIndex, float2 uv, float depth, float filterRadius, int sampleCount)
{
    float shadowFactor = 0.0f;
    for (int i = 0; i < sampleCount; i++)
    {
        float2 offset = g_SampleOffsets[i] * filterRadius;
        shadowFactor += cascadeShadowMap.SampleCmpLevelZero(g_SamplerShadow, float3(uv + offset, cascadeIndex), depth).r;
    }
    return shadowFactor / sampleCount;
}

// see https://developer.download.nvidia.com/whitepapers/2008/PCSS_Integration.pdf
float PCSS(Texture2DArray cascadeShadowMap, int cascadeIndex, float3 positionV)
{
    if (g_ShadowData.UseVogelDiskSample)
        VogelDiskSamples(positionV.xy, g_ShadowData.NumSamples);
    else
        PoissonDiskSamples(positionV.xy, g_ShadowData.NumSamples);
    
    float shadowFactor = 1.0f;
    
    float4 positionW = mul(float4(positionV, 1.0f), g_InvView);
    float4 positionH = mul(positionW, g_ShadowData.LightViewProj[cascadeIndex]);
    positionH.xyz /= positionH.w;
    
    float depth = positionH.z;
    
    // NDC space to texture space
    // [-1, 1] -> [0, 1] and flip the y-coord
    float2 shadowMapUV = (positionH.xy + 1.0f) * 0.5f;
    shadowMapUV.y = 1.0f - shadowMapUV.y;
   
    float avgBlockerDepth = FindBlocker(cascadeShadowMap, cascadeIndex, shadowMapUV, depth);

    if (avgBlockerDepth > 0)
    {
        float penumbraRatio = (depth - avgBlockerDepth) / avgBlockerDepth;
        float filterRadius = max(penumbraRatio * g_ShadowData.ShadowSoftness * LIGHT_SIZE, MIN_FILTER_RADIUS);
        
        shadowFactor = PCF(cascadeShadowMap, cascadeIndex, shadowMapUV, depth, filterRadius, g_ShadowData.NumSamples);
    }
    
    return shadowFactor;
}
    
float2 CascadeShadowWithPCSS(Texture2DArray cascadeShadowMap, float3 positionV)
{
    float ends[] = {
        g_ShadowData.CascadeEnds[0].x,
        g_ShadowData.CascadeEnds[0].y,
        g_ShadowData.CascadeEnds[0].z,
        g_ShadowData.CascadeEnds[0].w,
        g_ShadowData.CascadeEnds[1].x,
        g_ShadowData.CascadeEnds[1].y,
        g_ShadowData.CascadeEnds[1].z,
        g_ShadowData.CascadeEnds[1].w,
    };

    int cascadeIndex = -1;
    for (int i = NUM_CASCADES - 1; i >= 0; i--)
        if (positionV.z < ends[i + 1])
            cascadeIndex = i;
    
    float shadowFactor = 1.0f;
    if (cascadeIndex != -1)
    {
        shadowFactor = PCSS(cascadeShadowMap, cascadeIndex, positionV);
        
        // current depth value within the transition range,
        // blend between current and the next cascade
        if (cascadeIndex + 1 < NUM_CASCADES)
        {
            float currentCascadeNear = ends[cascadeIndex];
            float currentCascadeFar = ends[cascadeIndex + 1];
            float currentCascadeLength = currentCascadeFar - currentCascadeNear;
            
            float transitionLength = currentCascadeLength * g_ShadowData.TransitionRatio;
            float transitionStart = currentCascadeFar - transitionLength;
            
            if (positionV.z >= transitionStart)
            {
                float nextCascadeShadowFactor = PCSS(cascadeShadowMap, cascadeIndex + 1, positionV);
                float transitionFactor = saturate((positionV.z - transitionStart) / (currentCascadeFar - transitionStart));
                shadowFactor = lerp(shadowFactor, nextCascadeShadowFactor, transitionFactor);
            }
        }
    }
    
    return float2(shadowFactor, cascadeIndex);
}
