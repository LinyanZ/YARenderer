#ifndef __CONSTANT_BUFFERS_HLSL__
#define __CONSTANT_BUFFERS_HLSL__

struct Material
{
    float4 AmbientColor;
    //-------------------------- ( 16 bytes )
    float4 Albedo;
    //-------------------------- ( 16 bytes )
    float Metalness;
    float Roughness;
    float2 Padding;
    //-------------------------- ( 16 bytes )
    bool HasAlbedoTexture;
    bool HasNormalTexture;
    bool HasMetalnessTexture;
    bool HasRoughnessTexture;
    //-------------------------- ( 16 bytes )
};  //-------------------------- ( 16 * 5 = 80 bytes )

struct Light
{
    float4 PositionWS;
    //-------------------------- ( 16 bytes )
    float4 DirectionWS;
    //-------------------------- ( 16 bytes )
    float4 PositionVS;
    //-------------------------- ( 16 bytes )
    float4 DirectionVS;
    //-------------------------- ( 16 bytes )
    float4 Color;
    //-------------------------- ( 16 bytes )
    float SpotlightAngle;
    float Range;
    float Intensity;
    bool Enabled;
    //-------------------------- ( 16 bytes )
    bool Selected;
    uint Type;
    float2 Padding;
    //-------------------------- ( 16 bytes )
};  //-------------------------- ( 16 * 7 = 112 bytes )

struct ShadowData
{
    float4x4 LightViewProj[4];
    
    // Write float ...[4] as float4 ... 
    // since each 'float' gets 1 16-byte slot due to HLSL packing rule
    float4 CascadeRadius;
    float4 CascadeEnds[2];
    
    float TransitionRatio;
    float ShadowSoftness;
    bool ShowCascades;
    bool UseVogelDiskSample;
    int NumSamples;
};

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