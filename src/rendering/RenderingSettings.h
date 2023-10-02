#pragma once

enum class Antialising
{
	None = 0,
	FXAA,
	TAA
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
	bool EnableMotionBlur = true;
	float MotionBlurAmount = 0.25;

	// VXGI
	bool EnableGI = true;
	bool DebugVoxel = false;
	bool DynamicUpdate = true;
	int DebugVoxelMipLevel = 0;

	// Sun Light
	float SunTheta = 240;
	float SunPhi = 40;
	float SunLightIntensity = 3.0f;
};