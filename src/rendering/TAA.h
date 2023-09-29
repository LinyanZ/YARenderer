#pragma once

#include "dx/dx.h"
#include "dx/DxContext.h"
#include "dx/Texture.h"

class TAA
{
public:
	TAA(Ref<DxContext> dxContext, UINT width, UINT height);

	void Render(GraphicsCommandList commandList, Texture& velocityBuffer, bool isFirstFrame);
	void OnResize(UINT width, UINT height);

private:
	void BuildRootSignature();
	void BuildPSO();

private:
	Ref<DxContext> m_DxContext;
	Device m_Device;

	RootSignature m_RootSignature;
	PipelineState m_PSO;

	Texture m_SourceBuffer;
	Texture m_HistoryBuffer;
};