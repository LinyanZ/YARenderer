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

TextureCube gCubeMap : register(t0);

SamplerState gsamLinearWrap : register(s0);

struct VertexIn
{
    float3 PosL : POSITION;
};

struct VertexOut
{
    float3 PosL : POSITION;
    float4 PosH : SV_POSITION;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;

    // Use local vertex position as cubemap lookup vector.
    vout.PosL = vin.PosL;

	// World space remains the same.
    float4 posW = float4(vin.PosL, 1.0f);

	// Always center sky about camera.
    posW.xyz += gEyePosW;

	// Set z = w so that z/w = 1 (i.e., skydome always on far plane).
    vout.PosH = mul(posW, gViewProj).xyww;

    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    return gCubeMap.SampleLevel(gsamLinearWrap, pin.PosL, 0);
}