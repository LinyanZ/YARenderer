#include "common.hlsl"
#include "cube.hlsl"

cbuffer cbPass : register(b0)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gPrevViewProj;
    float4x4 gInvViewProj;
    float4x4 gViewProjTex;
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

cbuffer cbDebugSetting : register(b1)
{
    int MipLevel;
}

Texture3D<float4> albedo : register(t0);
SamplerState linearSampler : register(s0);

struct GeometryOut
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

uint VS(uint vertexID : SV_VertexID) : VERTEX_ID
{
    return vertexID;
}

uint3 Unflatten(uint i, uint dim)
{
    uint3 coord;
    
    coord[0] = i % dim;
    i /= dim;
    
    coord[1] = i % dim;
    i /= dim;
    
    coord[2] = i % dim;
    
    return coord;
}

float4 convRGBA8ToVec4(uint val)
{
    float4 re = float4(float((val & 0x000000FF)), float((val & 0x0000FF00) >> 8U), float((val & 0x00FF0000) >> 16U), float((val & 0xFF000000) >> 24U));
    return clamp(re, float4(0.0, 0.0, 0.0, 0.0), float4(255.0, 255.0, 255.0, 255.0));
}

[maxvertexcount(36)]
void GS(point uint input[1] : VERTEX_ID, 
        inout TriangleStream<GeometryOut> output)
{
    uint voxelDimension = VOXEL_DIMENSION / pow(2, MipLevel);
    float voxelGridSize = VOXEL_GRID_SIZE * pow(2, MipLevel);
    
    uint voxelIndex = input[0];
    uint3 coord = Unflatten(voxelIndex, voxelDimension);
    
    float3 uvw = (coord + 0.5) / voxelDimension;
    float3 center = uvw * 2 - 1;
    center.y *= -1;
    center *= voxelDimension;
    
    float4 color = albedo.Load(int4(coord, MipLevel));
    if (color.a <= 0)
        return;
	
    // Expand to a cube at this position.
    for (uint i = 0; i < 36; i += 3)
    {
        GeometryOut tri[3];
        
        for (uint j = 0; j < 3; ++j)
        {
            tri[j].position = float4(center, 1);
            tri[j].position.xyz += Cube[i + j].xyz;
            tri[j].position.xyz *= voxelGridSize;
            tri[j].position.xyz += VOXEL_GRID_WORLD_POS;
            tri[j].position = mul(float4(tri[j].position.xyz, 1), gViewProj);
            tri[j].color = color;
            output.Append(tri[j]);
        }
        
        output.RestartStrip();
    }
}

float4 PS(GeometryOut pin) : SV_Target
{
    return pin.color;
}