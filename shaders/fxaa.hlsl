// reference: http://blog.simonrodriguez.fr/articles/2016/07/implementing_fxaa.html

static const float EDGE_THRESHOLD_MIN = 0.0312;
static const float EDGE_THRESHOLD_MAX = 0.125;
static const int ITERATIONS = 12;
static const float SUBPIXEL_QUALITY = 0.75;

static const float QUALITY[12] = { 1.0, 1.0, 1.0, 1.0, 1.0, 1.5, 2.0, 2.0, 2.0, 2.0, 4.0, 8.0 };

cbuffer inverseScreenSize : register(b0)
{
    float inverseScreenSizeX;
    float inverseScreenSizeY;
};

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

float rgb2luma(float3 rgb)
{
    return sqrt(dot(rgb, float3(0.299, 0.587, 0.114)));
}

float4 PS(VertexOut pin) : SV_Target
{
    // the original color
    float3 colorCenter = sceneColor.Sample(defaultSampler, pin.texcoord).rgb;
    
    // luma at the current pixel
    float lumaCenter = rgb2luma(colorCenter);
    
    // luma at the four direct neighbours of the current pixel
    float lumaDown   = rgb2luma(sceneColor.Sample(defaultSampler, pin.texcoord, int2(0, -1)).rgb);
    float lumaUp     = rgb2luma(sceneColor.Sample(defaultSampler, pin.texcoord, int2(0, 1)).rgb);
    float lumaLeft   = rgb2luma(sceneColor.Sample(defaultSampler, pin.texcoord, int2(-1, 0)).rgb);
    float lumaRight  = rgb2luma(sceneColor.Sample(defaultSampler, pin.texcoord, int2(1, 0)).rgb);
    
    // find the maximum and minimum luma around the current pixel
    float lumaMin = min(lumaCenter, min(min(lumaDown, lumaUp), min(lumaLeft, lumaRight)));
    float lumaMax = max(lumaCenter, max(max(lumaDown, lumaUp), max(lumaLeft, lumaRight)));
    
    // compute the delta
    float lumaRange = lumaMax - lumaMin;
    
    // if the luma variation is lower than a threshold (or if we are in a really dark area)
    // we are not on an edge, don't perform any AA
    if (lumaRange < max(EDGE_THRESHOLD_MIN, lumaMax * EDGE_THRESHOLD_MAX))
        return float4(colorCenter, 1.0f);
    
    // query the 4 remaining corners lumas
    float lumaDownLeft   = rgb2luma(sceneColor.Sample(defaultSampler, pin.texcoord, int2(-1, -1)).rgb);
    float lumaDownRight  = rgb2luma(sceneColor.Sample(defaultSampler, pin.texcoord, int2(1, -1)).rgb);
    float lumaUpLeft     = rgb2luma(sceneColor.Sample(defaultSampler, pin.texcoord, int2(-1, 1)).rgb);
    float lumaUpRight    = rgb2luma(sceneColor.Sample(defaultSampler, pin.texcoord, int2(1, 1)).rgb);
    
    // combine the four edges lumas (using intermediary variables for
    // future computations with the same values)
    float lumaUpDown       = lumaUp + lumaDown;
    float lumaLeftRight    = lumaLeft + lumaRight;
    
    // same for corners
    float lumaLeftCorners  = lumaDownLeft + lumaUpLeft;
    float lumaRightCorners = lumaDownRight + lumaUpRight;
    float lumaUpCorners    = lumaUpLeft + lumaUpRight;
    float lumaDownCorners  = lumaDownLeft + lumaDownRight;
    
    // compute an estimation of the gradient along the horizontal and vertical axis
    float edgeHorizontal = abs(-2.0 * lumaLeft + lumaLeftCorners) + abs(-2.0 * lumaCenter + lumaUpDown) * 2.0    + abs(-2.0 * lumaRight + lumaRightCorners);
    float edgeVertical   = abs(-2.0 * lumaUp + lumaUpCorners)     + abs(-2.0 * lumaCenter + lumaLeftRight) * 2.0 + abs(-2.0 * lumaDown + lumaDownCorners);
    
    bool isHorizontal = edgeHorizontal > edgeVertical;
    
    // select the two neighboring texels lumas in the opposite direction to the local edge
    float luma1 = isHorizontal ? lumaDown : lumaLeft;
    float luma2 = isHorizontal ? lumaUp : lumaRight;
    
    // compute gradients in this direction
    float gradient1 = luma1 - lumaCenter;
    float gradient2 = luma2 - lumaCenter;
    
    // which direction is the steepest?
    bool is1Steepest = abs(gradient1) >= abs(gradient2);
    
    // gradient in the corresponding direction, normalized
    float gradientScaled = 0.25 * max(abs(gradient1), abs(gradient2));
    
    // choose the step size (one pixel) according to the edge direction
    float stepLength = isHorizontal ? inverseScreenSizeY : inverseScreenSizeX;
    
    // average luma in the correct direction
    float lumaLocalAverage;
    
    if (is1Steepest)
    {
        // switch the direction
        stepLength = -stepLength;
        lumaLocalAverage = 0.5 * (luma1 + lumaCenter);
    } 
    else
    {
        lumaLocalAverage = 0.5 * (luma2 + lumaCenter);
    }
    
    // shift UV in the correct direction by half a pixel
    float2 currentUV = pin.texcoord;
    
    if (isHorizontal)
        currentUV.y += stepLength * 0.5;
    else
        currentUV.x += stepLength * 0.5;
    
    // compute offset (for each iteration step) in the right direction
    float2 offset = isHorizontal ? float2(inverseScreenSizeX, 0.0) : float2(0.0, inverseScreenSizeY);
    
    // compute UVs to explore on each side of the edge, orthogonally
    // the QUALITY allows us to step further
    float2 uv1 = currentUV - offset;
    float2 uv2 = currentUV + offset;
    
    // read the lumas at both current extremities of the exploration segment
    // and compute the delta wrt to the local average luma
    float lumaEnd1 = rgb2luma(sceneColor.Sample(defaultSampler, uv1).rgb);
    float lumaEnd2 = rgb2luma(sceneColor.Sample(defaultSampler, uv2).rgb);
    lumaEnd1 -= lumaLocalAverage;
    lumaEnd2 -= lumaLocalAverage;
    
    // if the luma deltas at the current extremities are larger than the
    // local gradient, we have reached the side of the edge
    bool reached1 = abs(lumaEnd1) >= gradientScaled;
    bool reached2 = abs(lumaEnd2) >= gradientScaled;
    bool reachedBoth = reached1 && reached2;
    
    if (!reached1) 
        uv1 -= offset;
    if (!reached2)
        uv2 += offset;
    
    if (!reachedBoth)
    {
        for (int i = 2; i < ITERATIONS; i++)
        {
            if (!reached1)
            {
                lumaEnd1 = rgb2luma(sceneColor.Sample(defaultSampler, uv1).rgb);
                lumaEnd1 -= lumaLocalAverage;
            }
            
            if (!reached2)
            {
                lumaEnd2 = rgb2luma(sceneColor.Sample(defaultSampler, uv2).rgb);
                lumaEnd2 -= lumaLocalAverage;
            }
            
            // if the luma deltas at the current extremities are larger than the
            // local gradient, we have reached the side of the edge
            reached1 = abs(lumaEnd1) >= gradientScaled;
            reached2 = abs(lumaEnd2) >= gradientScaled;
            reachedBoth = reached1 && reached2;
            
            // if the side is not reached, we continue to explore in this 
            // direction, with a variable quality
            if (!reached1)
                uv1 -= offset * QUALITY[i];
            if (!reached2)
                uv2 += offset * QUALITY[i];
            
            // if both sides have been reached, stop the exploration
            if (reachedBoth)
                break;
        }
    }
    
    // compute the distances to each extremity of the edge
    float distance1 = isHorizontal ? (pin.texcoord.x - uv1.x) : (pin.texcoord.y - uv1.y);
    float distance2 = isHorizontal ? (uv2.x - pin.texcoord.x) : (uv2.y - pin.texcoord.y);
    
    // in which direction is the extremity of the edge closer?
    bool closerToDirection1 = distance1 < distance2;
    float distanceFinal = min(distance1, distance2);
    
    // length of the edge
    float edgeLength = distance1 + distance2;
    
    // UV offset: read in the direction of the closest side of the edge
    float pixelOffset = -distanceFinal / edgeLength + 0.5;
    
    // is the luma at center smaller than the local average?
    bool isLumaCenterSmaller = lumaCenter < lumaLocalAverage;
    
    // if the luma at center is smaller than at its neighbour,
    // the delta luma at each end should be positive (same variation)
    // (in the direction of the closer side of the edge)
    bool correctVariation = ((closerToDirection1 ? lumaEnd1 : lumaEnd2) < 0.0) != isLumaCenterSmaller;
    
    // if the luma variation is incorrect, do not offset
    float finalOffset = correctVariation ? pixelOffset : 0.0;
    
    // sub-pixel shifting
    // full weighted average of the luma over the 3x3 neighbourhood
    float lumaAverage = (1.0 / 12.0) * (2.0 * (lumaUpDown + lumaLeftRight) + lumaLeftCorners + lumaRightCorners);
    
    // ratio of the delta between the global average and the center luma, over the
    // luma range in the 3x3 neighbourhood
    float subPixelOffset = saturate(abs(lumaAverage - lumaCenter) / lumaRange);
    subPixelOffset = smoothstep(0, 1, subPixelOffset);
    
    // compute a sub-pixel offset based on this delta
    subPixelOffset = subPixelOffset * subPixelOffset * SUBPIXEL_QUALITY;
    
    // pick the biggest of the two offsets
    finalOffset = max(finalOffset, subPixelOffset);
    
    // compute the final UV coordinates
    float2 finalUV = pin.texcoord;
    if (isHorizontal)
        finalUV.y += finalOffset * stepLength;
    else
        finalUV.x += finalOffset * stepLength;
    
    return float4(sceneColor.Sample(defaultSampler, finalUV).rgb, 1.0);
}