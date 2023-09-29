#pragma once

#include "pch.h"

struct Light
{
	enum Type
	{
		PointLight = 0,
		SpotLight,
		DirectionalLight
	};

	XMFLOAT4 PositionWS;
	XMFLOAT4 DirectionWS;
	XMFLOAT4 PositionVS;
	XMFLOAT4 DirectionVS;
	XMFLOAT4 Color;

	float SpotlightAngle;
	float Range;
	float Intensity;

	BOOL Enabled;
	BOOL Selected;

	UINT Type;
	XMFLOAT2 Padding;
};