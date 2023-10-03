// reference: http://blog.simonrodriguez.fr/articles/2016/07/implementing_fxaa.html
#include "fullscreen.hlsl"
#include "samplers.hlsl"

#define NUM_SAMPLES 4

struct Resources
{
    uint InputTexIndex;
    uint VelocityTexIndex;
    float MotionBlurAmount;
};

ConstantBuffer<Resources> g_Resouces : register(b6);

float4 PS(VertexOut pin) : SV_Target
{
    Texture2D input = ResourceDescriptorHeap[g_Resouces.InputTexIndex];
    Texture2D velocity = ResourceDescriptorHeap[g_Resouces.VelocityTexIndex];

    float2 texCoord = pin.TexCoord;
    float2 motionVector = velocity.Sample(g_SamplerLinearClamp, texCoord).rg * float2(0.5, -0.5) * g_Resouces.MotionBlurAmount;
    
    float4 color = 0;
    float totalWeights = 0;
    
    for (int i = 0; i < NUM_SAMPLES; i++, texCoord += motionVector)
    {
        float weight = NUM_SAMPLES - i;
        color += input.Sample(g_SamplerLinearClamp, texCoord) * weight;
        totalWeights += weight;
    }
    
    return color / totalWeights;
}