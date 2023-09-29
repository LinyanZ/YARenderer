#pragma once

#include "core/MathHelper.h"
#include "dx/dx.h"
#include "dx/UploadBuffer.h"

#include "Material.h"
#include "Light.h"

struct ObjectConstants
{
	XMFLOAT4X4 World = MathHelper::Identity4x4();
	XMFLOAT4X4 PrevWorld = MathHelper::Identity4x4();
};

struct PassConstants
{
	XMFLOAT4X4 View = MathHelper::Identity4x4();
	XMFLOAT4X4 InvView = MathHelper::Identity4x4();
	XMFLOAT4X4 Proj = MathHelper::Identity4x4();
	XMFLOAT4X4 InvProj = MathHelper::Identity4x4();
	XMFLOAT4X4 ViewProjMatrix = MathHelper::Identity4x4();
	XMFLOAT4X4 PrevViewProjMatrix = MathHelper::Identity4x4();
	XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4();
	XMFLOAT4X4 ProjTex = MathHelper::Identity4x4();
	XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
	float cbPerObjectPad1 = 0.0f;
	XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
	XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };
	float NearZ = 0.0f;
	float FarZ = 0.0f;
	float TotalTime = 0.0f;
	float DeltaTime = 0.0f;
	XMFLOAT2 Jitter;
	XMFLOAT2 PreviousJitter;
	BOOL EnableGI;
};

struct SSAOConstants
{
	XMFLOAT4X4 Proj;
	XMFLOAT4X4 InvProj;
	XMFLOAT4X4 ProjTex;

	// For SsaoBlur.hlsl
	XMFLOAT4 BlurWeights[3];

	XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };

	// Coordinates given in view space.
	float OcclusionRadius = 0.5f;
	float OcclusionFadeStart = 0.2f;
	float OcclusionFadeEnd = 2.0f;
	float SurfaceEpsilon = 0.05f;
};

struct ShadowPassConstants
{
	XMFLOAT4X4 ViewProjMatrix[4];
	float CascadeRadius[4];
	float CascadeEnds[5];
	float Paddings[3];
	float TransitionRatio;
	float Softness;
	BOOL ShowCascades;
	BOOL UseVogelDiskSample;
	int NumSamples;
};

struct FrameResource
{
	FrameResource(Device device, UINT passCount, UINT objectCount, UINT materialCount, UINT lightCount)
	{
		PassCB = std::make_unique<UploadBuffer<PassConstants>>(device, passCount, true);
		ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, objectCount, true);
		MatCB = std::make_unique<UploadBuffer<MaterialConstants>>(device, materialCount, true);
		SSAOCB = std::make_unique<UploadBuffer<SSAOConstants>>(device, passCount, true);
		ShadowCB = std::make_unique<UploadBuffer<ShadowPassConstants>>(device, 1, true);
		LightBuffer = std::make_unique<UploadBuffer<Light>>(device, lightCount, false);
	}
	FrameResource(const FrameResource& rhs) = delete;
	FrameResource& operator=(const FrameResource& rhs) = delete;

	// We cannot update a cbuffer until the GPU is done processing the commands
	// that reference it.  So each frame needs their own cbuffers.
	std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
	std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;
	std::unique_ptr<UploadBuffer<MaterialConstants>> MatCB = nullptr;
	std::unique_ptr<UploadBuffer<SSAOConstants>> SSAOCB = nullptr;
	std::unique_ptr<UploadBuffer<ShadowPassConstants>> ShadowCB = nullptr;
	std::unique_ptr<UploadBuffer<Light>> LightBuffer = nullptr;

	UINT64 Fence = 0;
};
