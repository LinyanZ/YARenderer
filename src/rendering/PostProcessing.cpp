#include "pch.h"
#include "PostProcessing.h"
#include "PipelineStates.h"
#include "RenderingSettings.h"

#include "rendering/RenderingSettings.h"

extern RenderingSettings g_RenderingSettings;

PostProcessing::PostProcessing(Ref<DxContext> dxContext, UINT width, UINT height)
	: m_Device(dxContext->GetDevice()), m_Width(width), m_Height(height)
{
	for (int i = 0; i < 2; i++)
	{
		m_Textures[i] = Texture::Create(m_Device, width, height, 1, BACK_BUFFER_FORMAT, 1);

		m_Textures[i].Rtv = dxContext->GetRtvHeap().Alloc();
		m_Textures[i].Srv = dxContext->GetCbvSrvUavHeap().Alloc();

		m_Textures[i].CreateSrv(m_Device, D3D12_SRV_DIMENSION_TEXTURE2D, 0, 1);
		m_Textures[i].CreateRtv(m_Device, D3D12_RTV_DIMENSION_TEXTURE2D);
	}
}

void PostProcessing::OnResize(UINT width, UINT height)
{
	m_Width = width;
	m_Height = height;

	for (int i = 0; i < 2; i++)
	{
		m_Textures[i].Resize(m_Device, width, height);
		m_Textures[i].CreateSrv(m_Device, D3D12_SRV_DIMENSION_TEXTURE2D, 0, 1);
		m_Textures[i].CreateRtv(m_Device, D3D12_RTV_DIMENSION_TEXTURE2D);
	}
}

void PostProcessing::Render(GraphicsCommandList commandList, Texture &backBuffer, Texture &velocityBuffer)
{
	m_CurrTexture = -1;

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(backBuffer.Resource.Get(),
																		  D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON));

	if (g_RenderingSettings.EnableMotionBlur)
	{
		UINT resources[] = {velocityBuffer.Srv.Index, *reinterpret_cast<UINT *>(&g_RenderingSettings.MotionBlurAmount)};
		Pass(commandList, backBuffer, "motionBlur", resources, sizeof(resources));
	}

	if (g_RenderingSettings.AntialisingMethod == Antialising::FXAA)
	{
		Pass(commandList, backBuffer, "fxaa", nullptr, 0);
	}

	if (g_RenderingSettings.EnableToneMapping)
	{
		Pass(commandList, backBuffer, "toneMapping", reinterpret_cast<UINT *>(&g_RenderingSettings.Exposure), 1);
	}

	CopyToBackBuffer(commandList, backBuffer);
}

void PostProcessing::Pass(GraphicsCommandList commandList, Texture &backBuffer, const std::string &passName, UINT *addtionalResources, UINT numResources)
{
	auto &input = m_CurrTexture == -1 ? backBuffer : m_Textures[m_CurrTexture];
	m_CurrTexture = (m_CurrTexture + 1) % 2;
	auto &output = m_Textures[m_CurrTexture];

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(output.Resource.Get(),
																		  D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET));

	float clearValue[] = {0.0f, 0.0f, 0.0f, 0.0f};
	commandList->ClearRenderTargetView(output.Rtv.CPUHandle, clearValue, 0, nullptr);
	commandList->OMSetRenderTargets(1, &output.Rtv.CPUHandle, true, nullptr);

	commandList->SetPipelineState(PipelineStates::GetPSO(passName));

	std::vector<UINT> resources(1 + numResources);
	resources[0] = input.Srv.Index;
	for (int i = 0; i < numResources; i++)
		resources[i + 1] = addtionalResources[i];

	commandList->SetGraphicsRoot32BitConstants((UINT)RootParam::RenderResources, resources.size(), resources.data(), 0);

	commandList->DrawInstanced(3, 1, 0, 0);

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(output.Resource.Get(),
																		  D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON));
}

void PostProcessing::CopyToBackBuffer(GraphicsCommandList commandList, Texture &backBuffer)
{
	// no post processing steps have been done, no need to copy
	if (m_CurrTexture == -1)
	{
		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(backBuffer.Resource.Get(),
																			  D3D12_RESOURCE_STATE_COMMON,
																			  D3D12_RESOURCE_STATE_RENDER_TARGET));
		return;
	}

	auto &texture = m_Textures[m_CurrTexture];

	const D3D12_RESOURCE_BARRIER preCopyBarriers[] = {
		CD3DX12_RESOURCE_BARRIER::Transition(backBuffer.Resource.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST),
		CD3DX12_RESOURCE_BARRIER::Transition(texture.Resource.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_SOURCE)};
	const D3D12_RESOURCE_BARRIER postCopyBarriers[] = {
		CD3DX12_RESOURCE_BARRIER::Transition(backBuffer.Resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET),
		CD3DX12_RESOURCE_BARRIER::Transition(texture.Resource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON)};

	commandList->ResourceBarrier(2, preCopyBarriers);
	commandList->CopyResource(backBuffer.Resource.Get(), texture.Resource.Get());
	commandList->ResourceBarrier(2, postCopyBarriers);
}