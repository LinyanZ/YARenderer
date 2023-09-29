#pragma once

#include "dx/dx.h"
#include "dx/DxContext.h"
#include "dx/Descriptor.h"

class VolumeTexture
{
public:
	void Init(Ref<DxContext> dxContext, UINT size)
	{
		m_Device = dxContext->GetDevice();
		ViewPort = { 0.0f, 0.0f, (float)size, (float)size, 0.0f, 1.0f };
		ScissorRect = { 0, 0, (int)size, (int)size };

		m_MipLevels = 1;
		while (size >> m_MipLevels) m_MipLevels++;
		Uav.resize(m_MipLevels);

		Srv = dxContext->GetCbvSrvUavHeap().Alloc();
		for (int i = 0; i < m_MipLevels; i++)
			Uav[i] = dxContext->GetCbvSrvUavHeap().Alloc();

		OnResize(size, size, size);
	}

	void OnResize(UINT x, UINT y, UINT z)
	{
		if (X == x && Y == y && Z == z)
			return;

		X = x;
		Y = y;
		Z = z;

		BuildResource();
		BuildDescriptors();
	}

	UINT GetMipLevels() { return m_MipLevels; }

public:
	UINT X = -1, Y = -1, Z = -1;

	Descriptor Srv;
	std::vector<Descriptor> Uav;

	D3D12_VIEWPORT ViewPort;
	D3D12_RECT ScissorRect;

private:
	void BuildResource()
	{
		m_Resource = nullptr;

		D3D12_RESOURCE_DESC texDesc = {};
		texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
		texDesc.Alignment = 0;
		texDesc.Width = X;
		texDesc.Height = Y;
		texDesc.DepthOrArraySize = Z;
		texDesc.MipLevels = m_MipLevels;
		texDesc.Format = m_Format;
		texDesc.SampleDesc = { 1, 0 };
		texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		ThrowIfFailed(m_Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			nullptr,
			IID_PPV_ARGS(m_Resource.GetAddressOf())
		));
	}

	void BuildDescriptors()
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = m_SrvFormat;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
		srvDesc.Texture3D.MostDetailedMip = 0;
		srvDesc.Texture3D.MipLevels = m_MipLevels;
		srvDesc.Texture3D.ResourceMinLODClamp = 0.0f;

		m_Device->CreateShaderResourceView(m_Resource.Get(), &srvDesc, Srv.CPUHandle);

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = m_UavFormat;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
		uavDesc.Texture3D.FirstWSlice = 0;
		uavDesc.Texture3D.WSize = -1;
		for (int i = 0; i < m_MipLevels; i++)
		{
			uavDesc.Texture3D.MipSlice = i;
			m_Device->CreateUnorderedAccessView(m_Resource.Get(), nullptr, &uavDesc, Uav[i].CPUHandle);
		}
	}

private:
	Device m_Device;
	Resource m_Resource;

	DXGI_FORMAT m_SrvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT m_UavFormat = DXGI_FORMAT_R32_UINT;
	DXGI_FORMAT m_Format = DXGI_FORMAT_R8G8B8A8_TYPELESS;
	UINT m_MipLevels = 0;
};