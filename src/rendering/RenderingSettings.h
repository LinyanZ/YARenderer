#pragma once

enum class Antialising
{
	None = 0,
	FXAA,
	TAA
};

struct VXGISettings
{
	bool Enable = true;
	bool DynamicUpdate = true;
	bool SecondBounce = true;
	bool DebugVoxel = false;
	int DebugVoxelMipLevel = 0;
};

struct RenderingSettings
{
	// Display Settings
	bool EnableVSync = false;

	// Shadow Settings
	float MaxShadowDistance = 100.f;
	float CascadeRangeScale = 1.5f;
	float CascadeTransitionRatio = 0.2f;
	float ShadowSoftness = 0.6;
	bool ShowCascades = false;
	bool UseVogelDiskSample = true;
	int NumSamples = 32;

	// Post Processing
	Antialising AntialisingMethod = Antialising::TAA;
	bool EnableToneMapping = true;
	float Exposure = 0.3;
	bool EnableMotionBlur = true;
	float MotionBlurAmount = 0.5;

	VXGISettings GI;

	// Sun Light
	float SunTheta = 146;
	float SunPhi = 30;
	float SunLightIntensity = 8.0f;
	// float SunTheta = 240;
	// float SunPhi = 40;
	// float SunLightIntensity = 4.0f;
};