#pragma once

#include "pch.h"
#include "dx/dx.h"
#include "dx/Texture.h"

class ToneMapping
{
public:
	ToneMapping(Device device);

	void Render(GraphicsCommandList commandList, Texture& input);

private:
	void BuildRootSignature();
	void BuildPSO();

private:
	Device m_Device;

	RootSignature m_RootSig;
	PipelineState m_PSO;
};