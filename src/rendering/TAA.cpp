#include "pch.h"
#include "TAA.h"
#include "dx/Utils.h"
#include "PipelineStates.h"

TAA::TAA(Ref<DxContext> dxContext, UINT width, UINT height)
	: m_DxContext(dxContext), m_Device(dxContext->GetDevice())
{
	m_HistoryBuffer = Texture::Create(m_Device, width, height, 1, BACK_BUFFER_FORMAT, 1);
	m_SourceBuffer = Texture::Create(m_Device, width, height, 1, BACK_BUFFER_FORMAT, 1);

	m_HistoryBuffer.Srv = dxContext->GetCbvSrvUavHeap().Alloc();
	m_SourceBuffer.Srv = dxContext->GetCbvSrvUavHeap().Alloc();

	m_HistoryBuffer.CreateSrv(m_Device, D3D12_SRV_DIMENSION_TEXTURE2D, 0, 1);
	m_SourceBuffer.CreateSrv(m_Device, D3D12_SRV_DIMENSION_TEXTURE2D, 0, 1);
}

void TAA::Render(GraphicsCommandList commandList, Texture &velocityBuffer)
{
	auto &backBuffer = m_DxContext->CurrentBackBuffer();

	// No need to perform anything for the first frame
	if (!m_FirstFrame)
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

		commandList->SetPipelineState(PipelineStates::GetPSO("taa"));

		UINT resources[] = {
			m_SourceBuffer.Srv.Index,
			m_HistoryBuffer.Srv.Index,
			m_DxContext->DepthStencilBuffer().Srv.Index,
			velocityBuffer.Srv.Index};

		commandList->SetGraphicsRoot32BitConstants((UINT)RootParam::RenderResources, sizeof(resources), resources, 0);

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

	m_FirstFrame = false;
}

void TAA::OnResize(UINT width, UINT height)
{
	m_HistoryBuffer.Resize(m_Device, width, height);
	m_SourceBuffer.Resize(m_Device, width, height);

	m_HistoryBuffer.CreateSrv(m_Device, D3D12_SRV_DIMENSION_TEXTURE2D, 0, 1);
	m_SourceBuffer.CreateSrv(m_Device, D3D12_SRV_DIMENSION_TEXTURE2D, 0, 1);
}
