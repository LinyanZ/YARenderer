struct VertexOut
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};

Texture2D AmbientTexture : register(t0);
Texture2D SSAO : register(t1);

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

float4 PS(VertexOut pin) : SV_Target
{
    float3 ambientLight = AmbientTexture.Load(int3(pin.position.xy, 0)).xyz;
    float ambientAccess = SSAO.Sample(defaultSampler, pin.texcoord).r;
    return float4(ambientLight * ambientAccess, 1.0);
}