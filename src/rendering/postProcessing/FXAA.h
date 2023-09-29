#pragma once

#include "pch.h"
#include "dx/Texture.h"

class FXAA
{
public:
	FXAA(Device device);

	void Render(GraphicsCommandList commandList, Texture& input, UINT width, UINT height);

private:
	void BuildRootSignature();
	void BuildPSO();

private:
	Device m_Device;

	RootSignature m_RootSig;
	PipelineState m_PSO;
};