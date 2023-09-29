#pragma once

#include "pch.h"
#include "dx/dx.h"
#include "dx/DxContext.h"
#include "dx/Texture.h"

class RenderingUtils
{
public:
	static void Init(Ref<DxContext> dxContext);
	static void Cleanup();

	static Texture Equirect2Cubemap(Ref<DxContext> dxContext, Texture& inputTex);

	static Texture ComputeDiffuseIrradianceCubemap(Ref<DxContext> dxContext, Texture& inputTex);
	static Texture ComputePrefilteredSpecularEnvironmentMap(Ref<DxContext> dxContext, Texture& inputTex);
	static Texture ComputeBRDFLookUpTable(Ref<DxContext> dxContext);

	static void GenerateMipmaps(Ref<DxContext> dxContext, Texture& texture);

	static float Halton(UINT i, UINT b);

private:
	static std::unordered_map<std::string, RootSignature> m_RootSignatures;
	static std::unordered_map<std::string, PipelineState> m_PipelineStates;
};
