#pragma once

#include "pch.h"
#include "dx/DxContext.h"

class PostProcessing
{
public:
	PostProcessing(Ref<DxContext> dxContext, UINT width, UINT height);

	void OnResize(UINT width, UINT height);
	void Render(GraphicsCommandList commandList, Texture &backBuffer, Texture &velocityBuffer);

private:
	void Pass(GraphicsCommandList commandList, Texture &backBuffer,
			  const std::string &passName, UINT *resources, UINT numResources);
	void CopyToBackBuffer(GraphicsCommandList commandList, Texture &backBuffer);

private:
	Device m_Device;

	// Two for ping-ponging during different passes
	Texture m_Textures[2];
	UINT m_CurrTexture;

	UINT m_Width;
	UINT m_Height;
};