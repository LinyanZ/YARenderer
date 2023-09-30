#include "pch.h"
#include "ToneMapping.h"
#include "dx/Utils.h"

ToneMapping::ToneMapping(Device device)
	: m_Device(device)
{
	BuildRootSignature();
	BuildPSO();
}

void ToneMapping::Render(GraphicsCommandList commandList, Texture &input)
{
	commandList->SetGraphicsRootSignature(m_RootSig.Get());
	commandList->SetPipelineState(m_PSO.Get());
	commandList->SetGraphicsRootDescriptorTable(0, input.Srv.GPUHandle);

	commandList->DrawInstanced(3, 1, 0, 0);
}

void ToneMapping::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE srvTable;
	srvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

	CD3DX12_ROOT_PARAMETER rootParameters[1];
	rootParameters[0].InitAsDescriptorTable(1, &srvTable, D3D12_SHADER_VISIBILITY_PIXEL);

	CD3DX12_STATIC_SAMPLER_DESC samplerDesc{0, D3D12_FILTER_MIN_MAG_MIP_LINEAR};

	CD3DX12_ROOT_SIGNATURE_DESC desc(1, rootParameters,
									 1, &samplerDesc, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	m_RootSig = Utils::CreateRootSignature(m_Device, desc);
}

void ToneMapping::BuildPSO()
{
	auto VSByteCode = Utils::CompileShader(L"shaders\\tonemap.hlsl", nullptr, L"VS", L"vs_6_6");
	auto PSByteCode = Utils::CompileShader(L"shaders\\tonemap.hlsl", nullptr, L"PS", L"ps_6_6");

	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc;
	ZeroMemory(&desc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	desc.DepthStencilState.DepthEnable = false;
	desc.SampleMask = UINT_MAX;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = BACK_BUFFER_FORMAT;
	desc.SampleDesc = {1, 0};
	desc.DSVFormat = DEPTH_STENCIL_FORMAT;
	desc.pRootSignature = m_RootSig.Get();
	desc.VS = CD3DX12_SHADER_BYTECODE(VSByteCode->GetBufferPointer(), VSByteCode->GetBufferSize());
	desc.PS = CD3DX12_SHADER_BYTECODE(PSByteCode->GetBufferPointer(), PSByteCode->GetBufferSize());

	ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_PSO)));
}