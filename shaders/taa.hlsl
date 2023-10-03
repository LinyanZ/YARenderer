// ref: https://alextardif.com/TAA.html

#include "samplers.hlsl"

static const float FLT_EPS = 0.00000001f;
static const float PI = 3.141592653589793;

struct Resources
{
    uint SourceTexIndex;
    uint HistoryTexIndex;
    uint DepthTexIndex;
    uint VelocityTexIndex;
};

ConstantBuffer<Resources> g_Resources : register(b6);

struct VertexOut
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};

VertexOut VS(uint vertexID : SV_VertexID)
{
    VertexOut vout;
    
    // draw a triangle that covers the entire screen
    const float2 tex = float2(uint2(vertexID, vertexID << 1) & 2);
    vout.position = float4(lerp(float2(-1, 1), float2(1, -1), tex), 0, 1);
    vout.texcoord = tex;
    
    return vout;
}

// ref: https://github.com/TheRealMJP/MSAAFilter
float FilterBlackmanHarris(in float x)
{
    x = 1.0f - x;

    const float a0 = 0.35875f;
    const float a1 = 0.48829f;
    const float a2 = 0.14128f;
    const float a3 = 0.01168f;
    return saturate(a0 - a1 * cos(PI * x) + a2 * cos(2 * PI * x) - a3 * cos(3 * PI * x));
}

// ref: https://gist.github.com/TheRealMJP/c83b8c0f46b63f3a88a5986f4fa982b1
// Samples a texture with Catmull-Rom filtering, using 9 texture fetches instead of 16.
// See http://vec3.ca/bicubic-filtering-in-fewer-taps/ for more details
float4 SampleTextureCatmullRom(in Texture2D<float4> tex, in SamplerState linearSampler, in float2 uv, in float2 texSize)
{
    // We're going to sample a a 4x4 grid of texels surrounding the target UV coordinate. We'll do this by rounding
    // down the sample location to get the exact center of our "starting" texel. The starting texel will be at
    // location [1, 1] in the grid, where [0, 0] is the top left corner.
    float2 samplePos = uv * texSize;
    float2 texPos1 = floor(samplePos - 0.5f) + 0.5f;

    // Compute the fractional offset from our starting texel to our original sample location, which we'll
    // feed into the Catmull-Rom spline function to get our filter weights.
    float2 f = samplePos - texPos1;

    // Compute the Catmull-Rom weights using the fractional offset that we calculated earlier.
    // These equations are pre-expanded based on our knowledge of where the texels will be located,
    // which lets us avoid having to evaluate a piece-wise function.
    float2 w0 = f * (-0.5f + f * (1.0f - 0.5f * f));
    float2 w1 = 1.0f + f * f * (-2.5f + 1.5f * f);
    float2 w2 = f * (0.5f + f * (2.0f - 1.5f * f));
    float2 w3 = f * f * (-0.5f + 0.5f * f);

    // Work out weighting factors and sampling offsets that will let us use bilinear filtering to
    // simultaneously evaluate the middle 2 samples from the 4x4 grid.
    float2 w12 = w1 + w2;
    float2 offset12 = w2 / (w1 + w2);

    // Compute the final UV coordinates we'll use for sampling the texture
    float2 texPos0 = texPos1 - 1;
    float2 texPos3 = texPos1 + 2;
    float2 texPos12 = texPos1 + offset12;

    texPos0 /= texSize;
    texPos3 /= texSize;
    texPos12 /= texSize;

    float4 result = 0.0f;
    result += tex.SampleLevel(linearSampler, float2(texPos0.x, texPos0.y), 0.0f) * w0.x * w0.y;
    result += tex.SampleLevel(linearSampler, float2(texPos12.x, texPos0.y), 0.0f) * w12.x * w0.y;
    result += tex.SampleLevel(linearSampler, float2(texPos3.x, texPos0.y), 0.0f) * w3.x * w0.y;

    result += tex.SampleLevel(linearSampler, float2(texPos0.x, texPos12.y), 0.0f) * w0.x * w12.y;
    result += tex.SampleLevel(linearSampler, float2(texPos12.x, texPos12.y), 0.0f) * w12.x * w12.y;
    result += tex.SampleLevel(linearSampler, float2(texPos3.x, texPos12.y), 0.0f) * w3.x * w12.y;

    result += tex.SampleLevel(linearSampler, float2(texPos0.x, texPos3.y), 0.0f) * w0.x * w3.y;
    result += tex.SampleLevel(linearSampler, float2(texPos12.x, texPos3.y), 0.0f) * w12.x * w3.y;
    result += tex.SampleLevel(linearSampler, float2(texPos3.x, texPos3.y), 0.0f) * w3.x * w3.y;

    return result;
}

float Luminance(float3 color)
{
    return dot(color, float3(0.2127, 0.7152, 0.0722));
}

float4 PS(VertexOut pin) : SV_Target
{
    Texture2D source = ResourceDescriptorHeap[g_Resources.SourceTexIndex];
    Texture2D history = ResourceDescriptorHeap[g_Resources.HistoryTexIndex];
    Texture2D depth = ResourceDescriptorHeap[g_Resources.DepthTexIndex];
    Texture2D velocity = ResourceDescriptorHeap[g_Resources.VelocityTexIndex];

    int width, height, level;
    source.GetDimensions(0, width, height, level);
    
    float3 sourceSampleTotal = float3(0, 0, 0);
    float sourceSampleWeight = 0.0;
    
    float3 neighborhoodMin = 99999999.0;
    float3 neighborhoodMax = -99999999.0;
    
    float3 m1 = float3(0, 0, 0);
    float3 m2 = float3(0, 0, 0);
    
    float closestDepth = 99999999.0;
    int2 closestDepthPixelPosition = int2(0, 0);
    
    for (int x = -1; x <= 1; x++)
    {
        for (int y = -1; y <= 1; y++)
        {
            int2 pixelPosition = pin.texcoord * float2(width, height) + int2(x, y);
            pixelPosition = clamp(pixelPosition, 0, int2(width, height) - 1);
            
            float3 neighbor = max(0, source[pixelPosition].rgb);
            float subSampleDistance = length(float2(x, y));
            float subSampleWeight = FilterBlackmanHarris(subSampleDistance);    // TODO: Use Mi

            sourceSampleTotal += neighbor * subSampleWeight;
            sourceSampleWeight += subSampleWeight;
            
            neighborhoodMin = min(neighborhoodMin, neighbor);
            neighborhoodMax = max(neighborhoodMax, neighbor);
            
            m1 += neighbor;
            m2 += neighbor * neighbor;
            
            float currentDepth = depth[pixelPosition].r;
            if (currentDepth < closestDepth)
            {
                closestDepth = currentDepth;
                closestDepthPixelPosition = pixelPosition;
            }
        }
    }
    
    float2 motionVector = velocity[closestDepthPixelPosition].xy * float2(0.5, -0.5);
    float2 historyTexCoord = pin.texcoord - motionVector;
    
    float3 sourceSample = sourceSampleTotal / sourceSampleWeight;
    
    if (any(historyTexCoord != saturate(historyTexCoord)))
        return float4(sourceSample, 1.0);
    
    float3 historySample = SampleTextureCatmullRom(history, g_SamplerLinearWrap, historyTexCoord, float2(width, height)).rgb;
    
    float invSampleCount = 1.0 / 9.0;
    float gamma = 1.0;
    float3 mu = m1 * invSampleCount;
    float3 sigma = sqrt(abs((m2 * invSampleCount) - (mu * mu)));
    float3 minc = mu - gamma * sigma;
    float3 maxc = mu + gamma * sigma;
    
    historySample = clamp(historySample, neighborhoodMin, neighborhoodMax);
    
    float sourceWeight = 0.05;
    float historyWeight = 1.0 - sourceWeight;
    
    float3 compressedSource = sourceSample * rcp(max(max(sourceSample.r, sourceSample.g), sourceSample.b) + 1.0);
    float3 compressedHistory = historySample * rcp(max(max(historySample.r, historySample.g), historySample.b) + 1.0);
    float luminanceSource = Luminance(compressedSource);
    float luminanceHistory = Luminance(compressedHistory);
    
    sourceWeight *= 1.0 / (1.0 + luminanceSource);
    historyWeight *= 1.0 / (1.0 + luminanceHistory);
    
    float3 result = (sourceSample * sourceWeight + historySample * historyWeight) / max(sourceWeight + historyWeight, 0.00001);
    return float4(result, 1);
}