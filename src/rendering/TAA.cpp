#include "pch.h"
#include "TAA.h"
#include "dx/Utils.h"

TAA::TAA(Ref<DxContext> dxContext, UINT width, UINT height)
	: m_DxContext(dxContext), m_Device(dxContext->GetDevice())
{
	m_HistoryBuffer = Texture::Create(m_Device, width, height, 1, BACK_BUFFER_FORMAT, 1);
	m_SourceBuffer = Texture::Create(m_Device, width, height, 1, BACK_BUFFER_FORMAT, 1);

	m_HistoryBuffer.Srv = dxContext->GetCbvSrvUavHeap().Alloc();
	m_SourceBuffer.Srv = dxContext->GetCbvSrvUavHeap().Alloc();

	m_HistoryBuffer.CreateSrv(m_Device, D3D12_SRV_DIMENSION_TEXTURE2D, 0, 1);
	m_SourceBuffer.CreateSrv(m_Device, D3D12_SRV_DIMENSION_TEXTURE2D, 0, 1);

	BuildRootSignature();
	BuildPSO();
}

void TAA::Render(GraphicsCommandList commandList, Texture &velocityBuffer, bool isFirstFrame)
{
	auto &backBuffer = m_DxContext->CurrentBackBuffer();

	// No need to perform anything for the first frame
	if (!isFirstFrame)
	{
		// Copy the current back buffer to a temp resource for read
		const D3D12_RESOURCE_BARRIER preCopyBarriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(backBuffer.Resource.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE),
			CD3DX12_RESOURCE_BARRIER::Transition(m_SourceBuffer.Resource.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST)};
		const D3D12_RESOURCE_BARRIER postCopyBarriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(backBuffer.Resource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET),
			CD3DX12_RESOURCE_BARRIER::Transition(m_SourceBuffer.Resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON)};

		commandList->ResourceBarrier(2, preCopyBarriers);
		commandList->CopyResource(m_SourceBuffer.Resource.Get(), backBuffer.Resource.Get());
		commandList->ResourceBarrier(2, postCopyBarriers);

		commandList->SetGraphicsRootSignature(m_RootSignature.Get());
		commandList->SetPipelineState(m_PSO.Get());

		commandList->SetGraphicsRootDescriptorTable(0, m_SourceBuffer.Srv.GPUHandle);
		commandList->SetGraphicsRootDescriptorTable(1, m_HistoryBuffer.Srv.GPUHandle);
		commandList->SetGraphicsRootDescriptorTable(2, m_DxContext->DepthStencilBuffer().Srv.GPUHandle);
		commandList->SetGraphicsRootDescriptorTable(3, velocityBuffer.Srv.GPUHandle);

		commandList->DrawInstanced(3, 1, 0, 0);
	}

	// Copy the current frame to a history buffer for use in the next frame
	{
		const D3D12_RESOURCE_BARRIER preCopyBarriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(backBuffer.Resource.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE),
			CD3DX12_RESOURCE_BARRIER::Transition(m_HistoryBuffer.Resource.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST)};
		const D3D12_RESOURCE_BARRIER postCopyBarriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(backBuffer.Resource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET),
			CD3DX12_RESOURCE_BARRIER::Transition(m_HistoryBuffer.Resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON)};

		commandList->ResourceBarrier(2, preCopyBarriers);
		commandList->CopyResource(m_HistoryBuffer.Resource.Get(), backBuffer.Resource.Get());
		commandList->ResourceBarrier(2, postCopyBarriers);
	}
}

void TAA::OnResize(UINT width, UINT height)
{
	m_HistoryBuffer.Resize(m_Device, width, height);
	m_SourceBuffer.Resize(m_Device, width, height);

	m_HistoryBuffer.CreateSrv(m_Device, D3D12_SRV_DIMENSION_TEXTURE2D, 0, 1);
	m_SourceBuffer.CreateSrv(m_Device, D3D12_SRV_DIMENSION_TEXTURE2D, 0, 1);
}

void TAA::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE descriptorRanges[4] =
		{
			{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0}, // source
			{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0}, // history
			{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0}, // depth
			{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3, 0}, // velocity
		};

	CD3DX12_ROOT_PARAMETER slotRootParameter[4];
	slotRootParameter[0].InitAsDescriptorTable(1, &descriptorRanges[0], D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[1].InitAsDescriptorTable(1, &descriptorRanges[1], D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[2].InitAsDescriptorTable(1, &descriptorRanges[2], D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[3].InitAsDescriptorTable(1, &descriptorRanges[3], D3D12_SHADER_VISIBILITY_PIXEL);

	CD3DX12_STATIC_SAMPLER_DESC linearSamplerDesc{0, D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT};
	linearSamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	CD3DX12_ROOT_SIGNATURE_DESC desc(4, slotRootParameter, 1, &linearSamplerDesc,
									 D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	m_RootSignature = Utils::CreateRootSignature(m_Device, desc);
}

void TAA::BuildPSO()
{
	auto VSByteCode = Utils::CompileShader(L"shaders\\taa.hlsl", nullptr, L"VS", L"vs_6_6");
	auto PSByteCode = Utils::CompileShader(L"shaders\\taa.hlsl", nullptr, L"PS", L"ps_6_6");

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
	desc.SampleDesc = {1, 0};
	desc.RTVFormats[0] = BACK_BUFFER_FORMAT;
	desc.DSVFormat = DEPTH_STENCIL_FORMAT;
	desc.pRootSignature = m_RootSignature.Get();
	desc.VS = CD3DX12_SHADER_BYTECODE(VSByteCode->GetBufferPointer(), VSByteCode->GetBufferSize());
	desc.PS = CD3DX12_SHADER_BYTECODE(PSByteCode->GetBufferPointer(), PSByteCode->GetBufferSize());

	ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_PSO)));
}