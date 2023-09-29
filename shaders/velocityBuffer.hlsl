cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gPrevWorld;
};

cbuffer cbPass : register(b1)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gPrevViewProj;
    float4x4 gInvViewProj;
    float4x4 gProjTex;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
    float2 gJitter;
    float2 gPreviousJitter;
};

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
    
    float4 positionW = mul(float4(vin.Position, 1.0), gWorld);
    vout.PositionH = mul(positionW, gViewProj);
    
    vout.CurrPositionH = vout.PositionH;

    float4 prevPositionW = mul(float4(vin.Position, 1.0), gPrevWorld);
    vout.PrevPositionH = mul(prevPositionW, gPrevViewProj);
    
    return vout;
}

float2 PS(VertexOut pin) : SV_Target
{
    float3 currPosNDC = pin.CurrPositionH.xyz / pin.CurrPositionH.w;
    float3 prevPosNDC = pin.PrevPositionH.xyz / pin.PrevPositionH.w;
    
    float2 velocity = (currPosNDC.xy - gJitter) - (prevPosNDC.xy - gPreviousJitter);
    return velocity;
}