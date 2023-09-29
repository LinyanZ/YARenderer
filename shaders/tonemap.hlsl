// Tone-mapping & gamma correction.

#define GAMMA 2.2
#define EXPOSURE 0.2

struct VertexOut
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};

Texture2D sceneColor : register(t0);
SamplerState defaultSampler : register(s0);

VertexOut VS(uint vertexID : SV_VertexID)
{
    VertexOut vout;
    
    // draw a triangle that covers the entire screen
    const float2 tex = float2(uint2(vertexID, vertexID << 1) & 2);
    vout.position = float4(lerp(float2(-1, 1), float2(1, -1), tex), 0, 1);
    vout.texcoord = tex;
    
    return vout;
}

float3 ACESFilm(float3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float4 PS(VertexOut pin) : SV_Target
{
    // TODO: auto exposure?
    float3 color = sceneColor.Sample(defaultSampler, pin.texcoord).rgb;
    //return float4(color, 1);
    // ACES tonemapping
    float3 mappedColor = ACESFilm(color * EXPOSURE);
    return float4(pow(mappedColor, 1.0 / GAMMA), 1.0);
}