#include "pch.h"
#include "DxContext.h"

DxContext::DxContext(HWND hWnd, UINT width, UINT height)
	: m_hWnd(hWnd), m_Width(width), m_Height(height)
{
	EnableDebugLayer();

	CreateDXGIFactory();
	CheckTearingSupport();

	CreateDevice();
	m_CommandQueue = std::make_shared<CommandQueue>(m_Device, D3D12_COMMAND_LIST_TYPE_DIRECT);
	CreateSwapChain();

	CreateDescriptorHeaps();
	AllocateDescriptors();

	// do the initial resize code
	OnResize(width, height);
}

void DxContext::Present(bool vsync)
{
	// swap the back and front buffers
	int sync = vsync ? 1 : 0;
	int flag = vsync ? 0 : (m_TearingSupported ? DXGI_PRESENT_ALLOW_TEARING : 0);

	ThrowIfFailed(m_SwapChain->Present(sync, flag));

	m_CurrBackBuffer = (m_CurrBackBuffer + 1) % NUM_FRAMES_IN_FLIGHT;
}

GraphicsCommandList DxContext::GetCommandList()
{
	if (!m_ActiveCommandList)
		m_ActiveCommandList = m_CommandQueue->GetFreeCommandList();
	return m_ActiveCommandList;
}

UINT64 DxContext::ExecuteCommandList()
{
	ASSERT(m_ActiveCommandList, "No Active Command List.");

	UINT64 newFenceValue = m_CommandQueue->ExecuteCommandList(m_ActiveCommandList);
	m_ActiveCommandList = nullptr;
	return newFenceValue;
}

void DxContext::EnableDebugLayer()
{
#if defined(_DEBUG)
	// Always enable the debug layer before doing anything DX12 related
	// so all possible errors generated while creating DX12 objects
	// are caught by the debug layer.
	ComPtr<ID3D12Debug> debugInterface;
	ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
	debugInterface->EnableDebugLayer();
#endif
}

void DxContext::CreateDXGIFactory()
{
	UINT createFactoryFlags = 0;

#if defined(_DEBUG)
	createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

	ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&m_Factory)));
}

void DxContext::CreateDevice()
{
	ThrowIfFailed(D3D12CreateDevice(m_Adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_Device)));

	// enable debug message in debug mode
#if defined(_DEBUG)
	ComPtr<ID3D12InfoQueue> infoQueue;
	if (SUCCEEDED(m_Device.As(&infoQueue)))
	{
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

		// Suppress whole categories of messages
		// D3D12_MESSAGE_CATEGORY Categories[] = {};

		// Suppress messages based on their severity level
		D3D12_MESSAGE_SEVERITY Severities[] =
			{
				D3D12_MESSAGE_SEVERITY_INFO};

		// Suppress individual messages by their ID
		D3D12_MESSAGE_ID DenyIds[] = {
			D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE, // I'm really not sure how to avoid this message.
			D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,						  // This warning occurs when using capture frame while graphics debugging.
			D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,					  // This warning occurs when using capture frame while graphics debugging.
			D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,
		};

		D3D12_INFO_QUEUE_FILTER NewFilter = {};
		// NewFilter.DenyList.NumCategories = _countof(Categories);
		// NewFilter.DenyList.pCategoryList = Categories;
		NewFilter.DenyList.NumSeverities = _countof(Severities);
		NewFilter.DenyList.pSeverityList = Severities;
		NewFilter.DenyList.NumIDs = _countof(DenyIds);
		NewFilter.DenyList.pIDList = DenyIds;

		ThrowIfFailed(infoQueue->PushStorageFilter(&NewFilter));
	}
#endif
}

void DxContext::CheckTearingSupport()
{
	BOOL allowTearing = FALSE;

	ComPtr<IDXGIFactory5> factory5;
	if (SUCCEEDED(m_Factory.As(&factory5)))
	{
		if (FAILED(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing))))
		{
			allowTearing = FALSE;
		}
	}

	m_TearingSupported = allowTearing == TRUE;
	LOG_INFO("Tearing Supported: {0}", m_TearingSupported);
}

void DxContext::CreateSwapChain()
{
	// Release the previous swapchain we will be recreating.
	m_SwapChain.Reset();

	DXGI_SWAP_CHAIN_DESC1 desc = {};
	desc.Width = m_Width;
	desc.Height = m_Height;
	desc.Format = BACK_BUFFER_FORMAT;
	desc.Stereo = FALSE;
	desc.SampleDesc = {1, 0};
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.BufferCount = NUM_FRAMES_IN_FLIGHT;
	desc.Scaling = DXGI_SCALING_STRETCH;
	desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	desc.Flags = m_TearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

	ComPtr<IDXGISwapChain1> swapChain1;
	ThrowIfFailed(m_Factory->CreateSwapChainForHwnd(
		m_CommandQueue->GetCommandQueue().Get(),
		m_hWnd, &desc, nullptr, nullptr, &swapChain1));

	// Disable the Alt+Enter fullscreen toggle feature. Switching to fullscreen
	// will be handled manually.
	ThrowIfFailed(m_Factory->MakeWindowAssociation(m_hWnd, DXGI_MWA_NO_ALT_ENTER));

	ThrowIfFailed(swapChain1.As(&m_SwapChain));
}

void DxContext::CreateDescriptorHeaps()
{
	// +1 for the screen normal map, +2 for ambient maps, +2 for post processing, +5 for defered rendering, +1 for velocity buffer
	m_RtvHeap = DescriptorHeap::CreateDescriptorHeap(m_Device, {D3D12_DESCRIPTOR_HEAP_TYPE_RTV, NUM_FRAMES_IN_FLIGHT + 11, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0});
	// +1 for the depth stencil buffer, +4 for the cascade shadow map
	m_DsvHeap = DescriptorHeap::CreateDescriptorHeap(m_Device, {D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 5, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0});
	m_CbvSrvUavHeap = DescriptorHeap::CreateDescriptorHeap(m_Device, {D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1024, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 0});
	m_ImGuiHeap = DescriptorHeap::CreateDescriptorHeap(m_Device, {D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, NUM_FRAMES_IN_FLIGHT, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 0});
}

void DxContext::AllocateDescriptors()
{
	// allocate descriptors for back buffers
	for (int i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
	{
		m_SwapChainBuffer[i].Rtv = m_RtvHeap.Alloc();
		m_SwapChainBuffer[i].Srv = m_CbvSrvUavHeap.Alloc();
	}

	m_DepthStencilBuffer.Dsv = m_DsvHeap.Alloc();
	m_DepthStencilBuffer.Srv = m_CbvSrvUavHeap.Alloc();
}

void DxContext::OnResize(UINT width, UINT height)
{
	assert(m_Device);
	assert(m_SwapChain);

	m_Width = width;
	m_Height = height;

	m_CommandQueue->Flush();
	auto commandList = m_CommandQueue->GetFreeCommandList();

	// Release the previous resources we will be recreating.
	for (int i = 0; i < NUM_FRAMES_IN_FLIGHT; ++i)
		m_SwapChainBuffer[i].Resource.Reset();
	m_DepthStencilBuffer.Resource.Reset();

	ResizeSwapChain();
	ResizeDepthStencilBuffer(commandList);

	m_CommandQueue->ExecuteCommandList(commandList);
	m_CommandQueue->Flush();
}

void DxContext::ResizeSwapChain()
{
	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	ThrowIfFailed(m_SwapChain->GetDesc(&swapChainDesc));

	ThrowIfFailed(m_SwapChain->ResizeBuffers(
		NUM_FRAMES_IN_FLIGHT,
		m_Width, m_Height,
		swapChainDesc.BufferDesc.Format,
		swapChainDesc.Flags));

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = BACK_BUFFER_FORMAT;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;

	for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
	{
		auto &bufferTex = m_SwapChainBuffer[i];
		ThrowIfFailed(m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&bufferTex.Resource)));
		m_Device->CreateRenderTargetView(bufferTex.Resource.Get(), nullptr, bufferTex.Rtv.CPUHandle);
		m_Device->CreateShaderResourceView(bufferTex.Resource.Get(), &srvDesc, bufferTex.Srv.CPUHandle);
	}

	m_CurrBackBuffer = 0;
}

void DxContext::ResizeDepthStencilBuffer(GraphicsCommandList commandList)
{
	// Create the depth/stencil buffer and view.
	D3D12_RESOURCE_DESC depthStencilDesc = {};
	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilDesc.Alignment = 0;
	depthStencilDesc.Width = m_Width;
	depthStencilDesc.Height = m_Height;
	depthStencilDesc.DepthOrArraySize = 1;
	depthStencilDesc.MipLevels = 1;

	// Correction 11/12/2016: SSAO chapter requires an SRV to the depth buffer to read from
	// the depth buffer.  Therefore, because we need to create two views to the same resource:
	//   1. SRV format: DXGI_FORMAT_R24_UNORM_X8_TYPELESS
	//   2. DSV Format: DXGI_FORMAT_D24_UNORM_S8_UINT
	// we need to create the depth buffer resource with a typeless format.
	depthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;

	depthStencilDesc.SampleDesc = {1, 0};
	depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE optClear = {};
	optClear.Format = DEPTH_STENCIL_FORMAT;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;

	ThrowIfFailed(m_Device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&depthStencilDesc,
		D3D12_RESOURCE_STATE_COMMON,
		&optClear,
		IID_PPV_ARGS(m_DepthStencilBuffer.Resource.GetAddressOf())));

	// Create descriptor to mip level 0 of entire resource using the format of the resource.
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Format = DEPTH_STENCIL_FORMAT;
	dsvDesc.Texture2D.MipSlice = 0;
	m_Device->CreateDepthStencilView(m_DepthStencilBuffer.Resource.Get(), &dsvDesc, m_DepthStencilBuffer.Dsv.CPUHandle);

	// Transition the resource from its initial state to be used as a depth buffer.
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_DepthStencilBuffer.Resource.Get(),
																		  D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));

	// Create depth buffer shader resource view
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;

	m_Device->CreateShaderResourceView(m_DepthStencilBuffer.Resource.Get(), &srvDesc, m_DepthStencilBuffer.Srv.CPUHandle);
}

void DxContext::GetAdapter()
{
	ComPtr<IDXGIAdapter1> dxgiAdapter1;

	if (m_UseWarp)
	{
		ThrowIfFailed(m_Factory->EnumWarpAdapter(IID_PPV_ARGS(&dxgiAdapter1)));
		ThrowIfFailed(dxgiAdapter1.As(&m_Adapter));
	}
	else
	{
		// find the gpu with the largest memory
		SIZE_T maxDedicatedVideoMemory = 0;

		for (UINT i = 0; m_Factory->EnumAdapters1(i, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND; i++)
		{
			DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
			dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);

			if (
				// not a WARP adapter
				(dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
				// check if it's a directx12 compatible adapter
				SUCCEEDED(D3D12CreateDevice(dxgiAdapter1.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)) &&
				// has a larger video memory
				dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory)
			{
				maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
				ThrowIfFailed(dxgiAdapter1.As(&m_Adapter));
			}
		}
	}
}