#include "pch.h"
#include "FXAA.h"
#include "dx/Utils.h"

FXAA::FXAA(Device device)
	: m_Device(device)
{
	BuildRootSignature();
	BuildPSO();
}

void FXAA::Render(GraphicsCommandList commandList, Texture& input, UINT width, UINT height)
{
	commandList->SetGraphicsRootSignature(m_RootSig.Get());
	commandList->SetPipelineState(m_PSO.Get());
	commandList->SetGraphicsRootDescriptorTable(0, input.Srv.GPUHandle);

	float inverseScreenSize[2] = { 1.0 / width, 1.0 / height };
	commandList->SetGraphicsRoot32BitConstants(1, 2, inverseScreenSize, 0);

	commandList->DrawInstanced(3, 1, 0, 0);
}

void FXAA::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE srvTable;
	srvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

	CD3DX12_ROOT_PARAMETER rootParameters[2];
	rootParameters[0].InitAsDescriptorTable(1, &srvTable, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParameters[1].InitAsConstants(2, 0);

	CD3DX12_STATIC_SAMPLER_DESC computeSamplerDesc{ 0, D3D12_FILTER_MIN_MAG_MIP_LINEAR };

	CD3DX12_ROOT_SIGNATURE_DESC desc(2, rootParameters,
		1, &computeSamplerDesc, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	m_RootSig = Utils::CreateRootSignature(m_Device, desc);
}

void FXAA::BuildPSO()
{
	auto VSByteCode = Utils::CompileShader(L"shaders\\fxaa.hlsl", nullptr, "VS", "vs_5_0");
	auto PSByteCode = Utils::CompileShader(L"shaders\\fxaa.hlsl", nullptr, "PS", "ps_5_0");

	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
	desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	desc.DepthStencilState.DepthEnable = false;
	desc.SampleMask = UINT_MAX;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = BACK_BUFFER_FORMAT;
	desc.SampleDesc = { 1, 0 };
	desc.DSVFormat = DEPTH_STENCIL_FORMAT;
	desc.pRootSignature = m_RootSig.Get();
	desc.VS = CD3DX12_SHADER_BYTECODE(VSByteCode.Get());
	desc.PS = CD3DX12_SHADER_BYTECODE(PSByteCode.Get());

	ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_PSO)));
}