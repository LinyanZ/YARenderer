#ifndef __COMMON_HLSL__
#define __COMMON_HLSL__

#define POINT_LIGHT 0
#define SPOT_LIGHT 1
#define DIRECTIONAL_LIGHT 2

static const uint VOXEL_DIMENSION = 512;
static const float VOXEL_GRID_SIZE = 0.05;
static const float3 VOXEL_GRID_WORLD_POS = 0;
static const float VOXEL_COMPRESS_COLOR_RANGE = 10.0;

static const float PI = 3.141592653589793;
static const float TwoPI = 2 * PI;
static const float Epsilon = 0.00001;

// Constant normal incidence Fresnel factor for all dielectrics.
static const float3 Fdielectric = 0.04;

struct VertexIn
{
    float3 Position     : POSITION;
    float3 Normal       : NORMAL;
    float3 Tangent      : TANGENT;
    float3 Bitangent    : BITANGENT;
    float2 TexCoord     : TEXCOORD0;
};

struct VertexOut
{
    float3 PositionV    : POSITION0;        // View space position.
    float3 NormalV      : NORMAL;           // View space normal.
    float3 TangentV     : TANGENT;          // View space tangent.
    float3 BitangentV   : BITANGENT;        // View space bitangent.
    float2 TexCoord     : TEXCOORD0;        // Texture coordinate.
    float4 SsaoPosH     : TEXCOORD1;        // Texture space SSAO texture coordinate.
    float4 PositionH    : SV_POSITION;      // Clip space position.
};

#endif // __COMMON_HLSL__