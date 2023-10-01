#pragma once

#include "dx.h"

#include "CommandQueue.h"
#include "Texture.h"
#include "DescriptorHeap.h"

class DxContext
{
public:
	DxContext(HWND hWnd, UINT width, UINT height);

	void OnResize(UINT width, UINT height);
	void Present(bool vsync);

	Texture &CurrentBackBuffer() { return m_SwapChainBuffer[m_CurrBackBuffer]; }
	Texture &DepthStencilBuffer() { return m_DepthStencilBuffer; }
	Device GetDevice() { return m_Device; }

	DescriptorHeap &GetRtvHeap() { return m_RtvHeap; }
	DescriptorHeap &GetDsvHeap() { return m_DsvHeap; }
	DescriptorHeap &GetCbvSrvUavHeap() { return m_CbvSrvUavHeap; }
	DescriptorHeap &GetImGuiHeap() { return m_ImGuiHeap; }

	GraphicsCommandList GetCommandList();
	UINT64 ExecuteCommandList();
	UINT64 Signal() { return m_CommandQueue->Signal(); }
	void WaitForFenceValue(UINT64 fenceValue) { m_CommandQueue->WaitForFenceValue(fenceValue); }
	void Flush() { m_CommandQueue->Flush(); }

private:
	void EnableDebugLayer();
	void CreateDXGIFactory();
	void GetAdapter();
	void CreateDevice();
	void CheckTearingSupport();
	void CreateSwapChain();

	void CreateDescriptorHeaps();
	void AllocateDescriptors();

	void ResizeSwapChain();
	void ResizeDepthStencilBuffer(GraphicsCommandList commandList);

private:
	HWND m_hWnd;

	Factory m_Factory;
	Adapter m_Adapter;
	Device m_Device;

	Ref<CommandQueue> m_CommandQueue;
	GraphicsCommandList m_ActiveCommandList = nullptr;

	int m_CurrBackBuffer = 0;
	SwapChain m_SwapChain;
	Texture m_SwapChainBuffer[NUM_FRAMES_IN_FLIGHT];
	Texture m_DepthStencilBuffer;

	DescriptorHeap m_RtvHeap;
	DescriptorHeap m_DsvHeap;
	DescriptorHeap m_CbvSrvUavHeap;
	DescriptorHeap m_ImGuiHeap;

	bool m_UseWarp = false;
	bool m_TearingSupported = false;

	UINT m_Width;
	UINT m_Height;
};