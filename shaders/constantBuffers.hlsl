#ifndef __CONSTANT_BUFFERS_HLSL__
#define __CONSTANT_BUFFERS_HLSL__

#include "structs.hlsl"

cbuffer ObjectCB : register(b0)
{
    float4x4 g_World;
    float4x4 g_PrevWorld;
};

cbuffer PassCB : register(b1)
{
    float4x4 g_View;
    float4x4 g_InvView;
    float4x4 g_Proj;
    float4x4 g_InvProj;
    float4x4 g_ViewProj;
    float4x4 g_PrevViewProj;
    float4x4 g_InvViewProj;
    float4x4 g_ViewProjTex;
    float3 g_EyePosW;
    float _Pad1;
    float2 g_RenderTargetSize;
    float2 g_InvRenderTargetSize;
    float g_NearZ;
    float g_FarZ;
    float g_TotalTime;
    float g_DeltaTime;
    float2 g_Jitter;
    float2 g_PreviousJitter;
    bool g_EnableGI;
};

cbuffer MatCB : register(b2)
{
    Material g_Material; 
};

cbuffer ShadowCB : register(b4)
{
    ShadowData g_ShadowData;
};

#endif // __CONSTANT_BUFFERS_HLSL__