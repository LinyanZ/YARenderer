#pragma once

#include "pch.h"
#include "dx/DxContext.h"
#include "FXAA.h"
#include "ToneMapping.h"
#include "MotionBlur.h"

class PostProcessing
{
public:
	PostProcessing(Ref<DxContext> dxContext, UINT width, UINT height);

	void OnResize(UINT width, UINT height);
	void Render(GraphicsCommandList commandList, Texture& backBuffer, Texture& velocityBuffer);

private:
	void FXAAPass(GraphicsCommandList commandList, Texture& backBuffer);
	void ToneMappingPass(GraphicsCommandList commandList, Texture& backBuffer);
	void MotionBlurPass(GraphicsCommandList commandList, Texture& backBuffer, Texture& velocityBuffer);
	void CopyToBackBuffer(GraphicsCommandList commandList, Texture& backBuffer);

private:
	Device m_Device;

	// Two for ping-ponging during different passes
	Texture m_Textures[2];
	UINT m_CurrTexture;

	std::unique_ptr<FXAA> m_FXAA;
	std::unique_ptr<ToneMapping> m_ToneMapping;
	std::unique_ptr<MotionBlur> m_MotionBlur;

	UINT m_Width;
	UINT m_Height;
};