#include "fullscreen.hlsl"

#define GAMMA 2.2

struct Resources
{
    uint InputTexIndex;
    float Exposure;
};

ConstantBuffer<Resources> g_Resources : register(b6);

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
    Texture2D input = ResourceDescriptorHeap[g_Resources.InputTexIndex];

    // TODO: auto exposure?
    float3 color = input[pin.Position.xy].rgb;

    // ACES tonemapping
    float3 mappedColor = ACESFilm(color * g_Resources.Exposure);
    return float4(mappedColor, 1.0);

    return float4(pow(mappedColor, 1.0 / GAMMA), 1.0);
}