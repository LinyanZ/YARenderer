#include "common.hlsl"

cbuffer cbLightIndex : register(b0)
{
    uint gLightIndex;
};

cbuffer cbPass : register(b1)
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

StructuredBuffer<Light> Lights : register(t0);

float4x4 BuildWorldMatrix(Light light)
{
    // Calculate the translation matrix
    float4x4 translationMatrix = float4x4(
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        light.PositionWS.x, light.PositionWS.y, light.PositionWS.z, 1.0f
    );
    
    /*// Define the rotation angles in radians (modify the values as needed)
    float pitch = 0.0f; // X-axis rotation
    float yaw = 0.0f; // Y-axis rotation
    float roll = 0.0f; // Z-axis rotation

    // Calculate the rotation components
    float4x4 rotationMatrix = float4x4(
        cos(yaw) * cos(roll) + sin(pitch) * sin(yaw) * sin(roll), -cos(yaw) * sin(roll) + cos(roll) * sin(pitch) * sin(yaw), cos(pitch) * sin(yaw), 0.0f,
        cos(pitch) * sin(roll), cos(pitch) * cos(roll), -sin(pitch), 0.0f,
        -cos(roll) * sin(yaw) + cos(yaw) * sin(pitch) * sin(roll), cos(yaw) * cos(roll) * sin(pitch) + sin(yaw) * sin(roll), cos(pitch) * cos(yaw), 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    );*/

    // Define the scale (modify the values as needed)
    float3 scale = float3(light.Range.xxx);
    
    // Calculate the scale matrix
    float4x4 scaleMatrix = float4x4(
        scale.x, 0.0f, 0.0f, 0.0f,
        0.0f, scale.y, 0.0f, 0.0f,
        0.0f, 0.0f, scale.z, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    );

    // Combine the matrices in the correct order: Scale * Rotation * Translation
    return mul(scaleMatrix, translationMatrix);
}

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    
    float4x4 world = BuildWorldMatrix(Lights[gLightIndex]);
    float4 positionW = mul(float4(vin.Position, 1.0), world);
    
    // Transform to homogeneous clip space;
    vout.PositionH = mul(float4(positionW.xyz, 1.0), gViewProj);
    
    return vout;
}