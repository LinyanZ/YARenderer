// reference: http://blog.simonrodriguez.fr/articles/2016/07/implementing_fxaa.html

#define NUM_SAMPLES 4

struct VertexOut
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};

cbuffer Setting : register(b0)
{
    float MotionBlurAmount;
};

Texture2D source : register(t0);
Texture2D velocity : register(t1);

SamplerState linearSampler : register(s0);

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
    float2 texCoord = pin.texcoord;
    float2 motionVector = velocity.Sample(linearSampler, texCoord).rg * float2(0.5, -0.5) * MotionBlurAmount;
    
    float4 color = 0;
    float totalWeights = 0;
    
    for (int i = 0; i < NUM_SAMPLES; i++, texCoord += motionVector)
    {
        float weight = NUM_SAMPLES - i;
        color += source.Sample(linearSampler, texCoord) * weight;
        totalWeights += weight;
    }
    
    return color / totalWeights;
}