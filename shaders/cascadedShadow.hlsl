#include "constants.hlsl"
#include "structs.hlsl"
#include "samplers.hlsl"
#include "PCSS.hlsl"

#define NUM_CASCADES 4

int GetCascadeIndex(float3 positionV)
{
    // camera near plane and 4 cascade ends
    float ends[] = {
        g_ShadowData.CascadeEnds[0].x,
        g_ShadowData.CascadeEnds[0].y,
        g_ShadowData.CascadeEnds[0].z,
        g_ShadowData.CascadeEnds[0].w,
        g_ShadowData.CascadeEnds[1].x,
    };

    int cascadeIndex = -1;
    for (int i = NUM_CASCADES - 1; i >= 0; i--)
        if (positionV.z < ends[i + 1])
            cascadeIndex = i;

    return cascadeIndex;
}

float CascadedShadowWithPCSS(Texture2DArray cascadeShadowMap, int cascadeIndex, float3 positionV)
{
    // camera near plane and 4 cascade ends
    float ends[] = {
        g_ShadowData.CascadeEnds[0].x,
        g_ShadowData.CascadeEnds[0].y,
        g_ShadowData.CascadeEnds[0].z,
        g_ShadowData.CascadeEnds[0].w,
        g_ShadowData.CascadeEnds[1].x,
    };

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
    
    return shadowFactor;
}
