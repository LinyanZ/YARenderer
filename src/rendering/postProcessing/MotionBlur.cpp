#include "pch.h"
#include "MotionBlur.h"
#include "dx/Utils.h"

#include "rendering/RenderingSettings.h"

extern RenderingSettings g_RenderingSettings;

MotionBlur::MotionBlur(Device device)
	: m_Device(device)
{
	BuildRootSignature();
	BuildPSO();
}

void MotionBlur::Render(GraphicsCommandList commandList, Texture &input, Texture &velocityBuffer)
{
	commandList->SetGraphicsRootSignature(m_RootSig.Get());
	commandList->SetPipelineState(m_PSO.Get());

	commandList->SetGraphicsRootDescriptorTable(0, input.Srv.GPUHandle);
	commandList->SetGraphicsRootDescriptorTable(1, velocityBuffer.Srv.GPUHandle);
	commandList->SetGraphicsRoot32BitConstants(2, 1, &g_RenderingSettings.MotionBlurAmount, 0);

	commandList->DrawInstanced(3, 1, 0, 0);
}

void MotionBlur::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE descriptorRanges[2] =
		{
			{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0}, // source
			{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0}, // velocity
		};

	CD3DX12_ROOT_PARAMETER rootParameters[3];
	rootParameters[0].InitAsDescriptorTable(1, &descriptorRanges[0], D3D12_SHADER_VISIBILITY_PIXEL);
	rootParameters[1].InitAsDescriptorTable(1, &descriptorRanges[1], D3D12_SHADER_VISIBILITY_PIXEL);
	rootParameters[2].InitAsConstants(1, 0);

	CD3DX12_STATIC_SAMPLER_DESC samplerDesc{0, D3D12_FILTER_MIN_MAG_MIP_LINEAR};
	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;

	CD3DX12_ROOT_SIGNATURE_DESC desc(3, rootParameters,
									 1, &samplerDesc, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	m_RootSig = Utils::CreateRootSignature(m_Device, desc);
}

void MotionBlur::BuildPSO()
{
	auto VSByteCode = Utils::CompileShader(L"shaders\\motionBlur.hlsl", nullptr, L"VS", L"vs_6_6");
	auto PSByteCode = Utils::CompileShader(L"shaders\\motionBlur.hlsl", nullptr, L"PS", L"ps_6_6");

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
	desc.SampleDesc = {1, 0};
	desc.DSVFormat = DEPTH_STENCIL_FORMAT;
	desc.pRootSignature = m_RootSig.Get();
	desc.VS = CD3DX12_SHADER_BYTECODE(VSByteCode->GetBufferPointer(), VSByteCode->GetBufferSize());
	desc.PS = CD3DX12_SHADER_BYTECODE(PSByteCode->GetBufferPointer(), PSByteCode->GetBufferSize());

	ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_PSO)));
}