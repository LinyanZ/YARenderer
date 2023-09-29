#pragma once

#include "pch.h"
#include "dx/Texture.h"

struct MaterialConstants
{
	XMFLOAT4 AmbientColor;
	XMFLOAT4 Albedo;
	float Metalness;
	float Roughness;
	XMFLOAT2 Padding;

	BOOL HasAlbedoTexture;
	BOOL HasNormalTexture;
	BOOL HasMetalnessTexture;
	BOOL HasRoughnessTexture;
};

class Material
{
public:
	MaterialConstants BuildMaterialConstants()
	{
		return MaterialConstants{ AmbientColor, Albedo, Metalness, Roughness, XMFLOAT2{},
			HasAlbedoTexture, HasNormalTexture, HasMetalnessTexture, HasRoughnessTexture };
	}

	XMFLOAT4 AmbientColor = XMFLOAT4(0.1f, 0.1f, 0.1f, 1.f);
	XMFLOAT4 Albedo = XMFLOAT4(0.7f, 0.7f, 0.7f, 1.f);
	float Metalness = 0.0f;
	float Roughness = 0.8f;

	BOOL HasAlbedoTexture = FALSE;
	BOOL HasNormalTexture = FALSE;
	BOOL HasMetalnessTexture = FALSE;
	BOOL HasRoughnessTexture = FALSE;

	std::string AlbedoFilename;
	std::string NormalFilename;
	std::string MetalnessFilename;
	std::string RoughnessFilename;

	Texture AlbedoTexture;
	Texture NormalTexture;
	Texture MetalnessTexture;
	Texture RoughnessTexture;
};