#include "pch.h"
#include "PostProcessing.h"
#include "../RenderingSettings.h"

#include "rendering/RenderingSettings.h"

extern RenderingSettings g_RenderingSettings;

PostProcessing::PostProcessing(Ref<DxContext> dxContext, UINT width, UINT height)
	: m_Device(dxContext->GetDevice()), m_Width(width), m_Height(height)
{
	m_FXAA = std::make_unique<FXAA>(m_Device);
	m_ToneMapping = std::make_unique<ToneMapping>(m_Device);
	m_MotionBlur = std::make_unique<MotionBlur>(m_Device);

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

void PostProcessing::Render(GraphicsCommandList commandList, Texture& backBuffer, Texture& velocityBuffer)
{
	m_CurrTexture = -1;

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(backBuffer.Resource.Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON));

	if (g_RenderingSettings.EnableMotionBlur) MotionBlurPass(commandList, backBuffer, velocityBuffer);
	if (g_RenderingSettings.AntialisingMethod == Antialising::FXAA) FXAAPass(commandList, backBuffer);
	if (g_RenderingSettings.EnableToneMapping) ToneMappingPass(commandList, backBuffer);

	CopyToBackBuffer(commandList, backBuffer);
}

void PostProcessing::FXAAPass(GraphicsCommandList commandList, Texture& backBuffer)
{
	auto& input = m_CurrTexture == -1 ? backBuffer : m_Textures[m_CurrTexture];
	m_CurrTexture = (m_CurrTexture + 1) % 2;
	auto& output = m_Textures[m_CurrTexture];

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(output.Resource.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET));

	float clearValue[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	commandList->ClearRenderTargetView(output.Rtv.CPUHandle, clearValue, 0, nullptr);
	commandList->OMSetRenderTargets(1, &output.Rtv.CPUHandle, true, nullptr);

	m_FXAA->Render(commandList, input, m_Width, m_Height);

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(output.Resource.Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON));
}

void PostProcessing::ToneMappingPass(GraphicsCommandList commandList, Texture& backBuffer)
{
	auto& input = m_CurrTexture == -1 ? backBuffer : m_Textures[m_CurrTexture];
	m_CurrTexture = (m_CurrTexture + 1) % 2;
	auto& output = m_Textures[m_CurrTexture];

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(output.Resource.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET));

	float clearValue[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	commandList->ClearRenderTargetView(output.Rtv.CPUHandle, clearValue, 0, nullptr);
	commandList->OMSetRenderTargets(1, &output.Rtv.CPUHandle, true, nullptr);

	m_ToneMapping->Render(commandList, input);

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(output.Resource.Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON));
}

void PostProcessing::MotionBlurPass(GraphicsCommandList commandList, Texture& backBuffer, Texture& velocityBuffer)
{
	auto& input = m_CurrTexture == -1 ? backBuffer : m_Textures[m_CurrTexture];
	m_CurrTexture = (m_CurrTexture + 1) % 2;
	auto& output = m_Textures[m_CurrTexture];

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(output.Resource.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET));

	float clearValue[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	commandList->ClearRenderTargetView(output.Rtv.CPUHandle, clearValue, 0, nullptr);
	commandList->OMSetRenderTargets(1, &output.Rtv.CPUHandle, true, nullptr);

	m_MotionBlur->Render(commandList, input, velocityBuffer);

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(output.Resource.Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON));
}

void PostProcessing::CopyToBackBuffer(GraphicsCommandList commandList, Texture& backBuffer)
{
	auto& texture = m_Textures[m_CurrTexture];

	const D3D12_RESOURCE_BARRIER preCopyBarriers[] = {
		CD3DX12_RESOURCE_BARRIER::Transition(backBuffer.Resource.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST),
		CD3DX12_RESOURCE_BARRIER::Transition(texture.Resource.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_SOURCE)
	};
	const D3D12_RESOURCE_BARRIER postCopyBarriers[] = {
		CD3DX12_RESOURCE_BARRIER::Transition(backBuffer.Resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET),
		CD3DX12_RESOURCE_BARRIER::Transition(texture.Resource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON)
	};

	commandList->ResourceBarrier(2, preCopyBarriers);
	commandList->CopyResource(backBuffer.Resource.Get(), texture.Resource.Get());
	commandList->ResourceBarrier(2, postCopyBarriers);
}