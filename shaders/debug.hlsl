struct VertexOut
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};

cbuffer debugSlot : register(b0)
{
    int slot;
}

Texture2D texColor : register(t0);
SamplerState defaultSampler : register(s0);

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
    
    //// draw a triangle that covers the entire screen
    //const float2 tex = float2(uint2(vertexID, vertexID << 1) & 2);
    //vout.position = float4(lerp(float2(-1, 1), float2(1, -1), tex), 0, 1);
    //vout.texcoord = tex;
    
    //vout.position = float4(vout.position.xy * 0.25, vout.position.zw);
    
    vout.texcoord = gTexCoords[vertexID];

    vout.position = float4(0.5 * vout.texcoord.x - 1.0f, 1.0f - 0.5f * vout.texcoord.y, 0.0f, 1.0f);
    
    vout.position.x += (slot / 4) * 0.5;
    vout.position.y -= (slot % 4) * 0.5;
    
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    float4 color = texColor.Sample(defaultSampler, pin.texcoord);
    //if (color.g == 0.0 && color.b == 0.0)
    //    return float4(color.rrr, 1.0);
    
    //return float4(color.rg, 0.0, 1.0);
    return float4(color.rgb, 1.0);
}