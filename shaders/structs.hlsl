#ifndef __STRUCTS_HLSL__
#define __STRUCTS_HLSL__

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

#endif // __STRUCTS_HLSL__