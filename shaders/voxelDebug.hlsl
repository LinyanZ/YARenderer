#include "common.hlsl"
#include "cube.hlsl"
#include "constantBuffers.hlsl"

struct Resources
{
    uint TexIndex;
    uint MipLevel;
};

ConstantBuffer<Resources> g_Resources : register(b6);

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
    uint voxelDimension = VOXEL_DIMENSION / pow(2, g_Resources.MipLevel);
    float voxelGridSize = VOXEL_GRID_SIZE * pow(2, g_Resources.MipLevel);
    
    uint voxelIndex = input[0];
    uint3 coord = Unflatten(voxelIndex, voxelDimension);
    
    float3 uvw = (coord + 0.5) / voxelDimension;
    float3 center = uvw * 2 - 1;
    center.y *= -1;
    center *= voxelDimension;
    
    Texture3D<float4> voxels = ResourceDescriptorHeap[g_Resources.TexIndex];
    float4 color = voxels.Load(int4(coord, g_Resources.MipLevel));
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
            tri[j].position = mul(float4(tri[j].position.xyz, 1), g_ViewProj);
            tri[j].color = color;
            output.Append(tri[j]);
        }
        
        output.RestartStrip();
    }
}

float4 PS(GeometryOut pin) : SV_Target
{
    // return float4(1,1,1,1);
    return pin.color;
}