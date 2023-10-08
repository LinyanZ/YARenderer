#include "samplers.hlsl"

struct VertexOut
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};

struct Resources
{
    uint DebugTexIndex;
    uint Slot;
};

ConstantBuffer<Resources> g_Resources : register(b6);

static const float2 gTexCoords[6] =
{
    float2(0.0f, 1.0f),
    float2(0.0f, 0.0f),
    float2(1.0f, 0.0f),
    float2(0.0f, 1.0f),
    float2(1.0f, 0.0f),
    float2(1.0f, 1.0f)
};

VertexOut VS(uint vertexID : SV_VertexID)
{
    VertexOut vout;
    vout.texcoord = gTexCoords[vertexID];

    vout.position = float4(0.5 * vout.texcoord.x - 1.0f, 1.0f - 0.5f * vout.texcoord.y, 0.0f, 1.0f);
    
    vout.position.x += (g_Resources.Slot / 4) * 0.5;
    vout.position.y -= (g_Resources.Slot % 4) * 0.5;
    
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    Texture2D tex = ResourceDescriptorHeap[g_Resources.DebugTexIndex];
    float4 color = tex.Sample(g_SamplerPointWrap, pin.texcoord);
    return float4(color.rgb, 1.0);
}