#pragma once

#include "pch.h"
#include "dx/DxContext.h"

class PipelineStateManager
{
public:
	PipelineStateManager(Device device);

	PipelineState GetPSO(std::string name = "default") { return m_PSOs[name]; }
	RootSignature GetRootSignature(std::string name = "default") { return m_RootSignatures[name]; }

private:
	void BuildShadersAndInputLayouts();
	void BuildRootSignatures();
	void BuildPSOs();
	std::vector<CD3DX12_STATIC_SAMPLER_DESC> GetStaticSamplers();

private:
	std::unordered_map<std::string, PipelineState> m_PSOs;
	std::unordered_map<std::string, RootSignature> m_RootSignatures;

	std::unordered_map<std::string, Blob> m_VSByteCodes;
	std::unordered_map<std::string, Blob> m_PSByteCodes;
	std::unordered_map<std::string, Blob> m_CSByteCodes;

	std::unordered_map<std::string, std::vector<D3D12_INPUT_ELEMENT_DESC>> m_InputLayouts;

	Device m_Device;
};
