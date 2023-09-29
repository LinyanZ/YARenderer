#pragma once

#include "dx/dx.h"
#include "dx/Texture.h"

class MotionBlur
{
public:
	MotionBlur(Device device);

	void Render(GraphicsCommandList commandList, Texture& input, Texture& velocityBuffer);

private:
	void BuildRootSignature();
	void BuildPSO();

private:
	Device m_Device;

	RootSignature m_RootSig;
	PipelineState m_PSO;
};