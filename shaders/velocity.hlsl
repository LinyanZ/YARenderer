#include "constantBuffers.hlsl"

struct VertexIn
{
    float3 Position         : POSITION;
    float3 Normal           : NORMAL;
    float3 Tangent          : TANGENT;
    float3 Bitangent        : BITANGENT;
    float2 TexCoord         : TEXCOORD0;
};

struct VertexOut
{
    float4 PositionH        : SV_POSITION;      // Clip space position.
    float4 CurrPositionH    : POSITION0;        // Current clip space position. Using SV_POSITION directly results in strange result.
    float4 PrevPositionH    : POSITION1;        // Previous clip space position.
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    
    float4 positionW = mul(float4(vin.Position, 1.0), g_World);
    vout.PositionH = mul(positionW, g_ViewProj);
    
    vout.CurrPositionH = vout.PositionH;

    float4 prevPositionW = mul(float4(vin.Position, 1.0), g_PrevWorld);
    vout.PrevPositionH = mul(prevPositionW, g_PrevViewProj);
    
    return vout;
}

float2 PS(VertexOut pin) : SV_Target
{
    float3 currPosNDC = pin.CurrPositionH.xyz / pin.CurrPositionH.w;
    float3 prevPosNDC = pin.PrevPositionH.xyz / pin.PrevPositionH.w;
    
    float2 velocity = (currPosNDC.xy - g_Jitter) - (prevPosNDC.xy - g_PreviousJitter);
    return velocity;
}