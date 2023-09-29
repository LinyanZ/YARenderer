#pragma once

#include "pch.h"
#include "asset/Image.h"
#include "DescriptorHeap.h"

struct Texture
{
	static Texture Create(Device device, UINT width, UINT height, UINT depth, DXGI_FORMAT format, UINT levels = 0);
	static Texture Create(Device device, GraphicsCommandList commandList,
		Ref<Image>& image, DXGI_FORMAT format, UINT levels = 0);

	void Resize(Device device, UINT width, UINT height);

	static UINT NumMipmapLevels(UINT width, UINT height);

	void CreateRtv(Device device, D3D12_RTV_DIMENSION dimension, UINT mipSlice = 0, UINT planeSlice = 0);
	void CreateSrv(Device device, D3D12_SRV_DIMENSION dimension, UINT mostDetailedMip = 0, UINT mipLevels = 0);
	void CreateUav(Device device, UINT mipSlice);

	UINT Width, Height, Levels;

	ComPtr<ID3D12Resource> Resource = nullptr;
	ComPtr<ID3D12Resource> UploadHeap = nullptr;

	Descriptor Rtv;
	Descriptor Dsv;
	Descriptor Srv;
	Descriptor Uav;
};
