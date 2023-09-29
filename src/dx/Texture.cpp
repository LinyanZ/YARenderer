#include "Texture.h"

Texture Texture::Create(Device device, UINT width, UINT height, UINT depth, DXGI_FORMAT format, UINT levels)
{
	ASSERT(depth == 1 || depth == 6, "Texture depth not equal to 1 or 6: depth = {}", depth);

	Texture texture;
	texture.Width = width;
	texture.Height = height;
	texture.Levels = (levels > 0) ? levels : NumMipmapLevels(width, height);

	D3D12_RESOURCE_DESC desc = {};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Width = width;
	desc.Height = height;
	desc.DepthOrArraySize = depth;
	desc.MipLevels = levels;
	desc.Format = format;
	desc.SampleDesc = {1, 0};
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&texture.Resource)));

	return texture;
}

Texture Texture::Create(Device device, GraphicsCommandList commandList, Ref<Image> &image, DXGI_FORMAT format, UINT levels)
{
	Texture texture = Create(device, image->Width(), image->Height(), 1, format, levels);

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(texture.Resource.Get(),
																		  D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));

	// create an upload heap and upload the texture
	// reference: https://www.braynzarsoft.net/viewtutorial/q16390-directx-12-textures-from-file

	UINT64 textureUploadBufferSize;
	device->GetCopyableFootprints(&texture.Resource->GetDesc(), 0, 1, 0, nullptr, nullptr, nullptr, &textureUploadBufferSize);

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(textureUploadBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(texture.UploadHeap.GetAddressOf())));

	D3D12_SUBRESOURCE_DATA sub = {};
	sub.pData = image->Pixels<void>();
	sub.RowPitch = image->Pitch();
	sub.SlicePitch = image->Pitch() * image->Height();

	UpdateSubresources(commandList.Get(), texture.Resource.Get(), texture.UploadHeap.Get(), 0, 0, 1, &sub);

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(texture.Resource.Get(),
																		  D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON));

	return texture;
}

void Texture::Resize(Device device, UINT width, UINT height)
{
	Width = width;
	Height = height;

	D3D12_RESOURCE_DESC desc = Resource->GetDesc();
	desc.Width = width;
	desc.Height = height;

	Resource = nullptr;

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&Resource)));
}

UINT Texture::NumMipmapLevels(UINT width, UINT height)
{
	UINT levels = 1;
	while ((width | height) >> levels)
		levels++;
	return levels;
}

void Texture::CreateRtv(Device device, D3D12_RTV_DIMENSION dimension, UINT mipSlice, UINT planeSlice)
{
	const D3D12_RESOURCE_DESC desc = Resource->GetDesc();

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = desc.Format;
	rtvDesc.Texture2D.MipSlice = mipSlice;
	rtvDesc.Texture2D.PlaneSlice = planeSlice;
	device->CreateRenderTargetView(Resource.Get(), &rtvDesc, Rtv.CPUHandle);
}

void Texture::CreateSrv(Device device, D3D12_SRV_DIMENSION dimension, UINT mostDetailedMip, UINT mipLevels)
{
	const D3D12_RESOURCE_DESC desc = Resource->GetDesc();
	const UINT effectiveMipLevels = (mipLevels > 0) ? mipLevels : (desc.MipLevels - mostDetailedMip);
	ASSERT(!(desc.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE), "Failed to create shader resource view: "
																	 "Texture created with D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE flag.");

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = desc.Format;
	srvDesc.ViewDimension = dimension;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	switch (dimension)
	{
	case D3D12_SRV_DIMENSION_TEXTURE2D:
		srvDesc.Texture2D.MostDetailedMip = mostDetailedMip;
		srvDesc.Texture2D.MipLevels = effectiveMipLevels;
		break;
	case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
		srvDesc.Texture2DArray.MostDetailedMip = mostDetailedMip;
		srvDesc.Texture2DArray.MipLevels = effectiveMipLevels;
		srvDesc.Texture2DArray.FirstArraySlice = 0;
		srvDesc.Texture2DArray.ArraySize = desc.DepthOrArraySize;
		break;
	case D3D12_SRV_DIMENSION_TEXTURECUBE:
		ASSERT(desc.DepthOrArraySize == 6, "SRV dimension is set to TEXTURECUBE, but texture has depth not equal to 6.");
		srvDesc.TextureCube.MostDetailedMip = mostDetailedMip;
		srvDesc.TextureCube.MipLevels = effectiveMipLevels;
		break;
	default:
		ASSERT(false, "SRV dimension not supported.");
	}

	device->CreateShaderResourceView(Resource.Get(), &srvDesc, Srv.CPUHandle);
}

void Texture::CreateUav(Device device, UINT mipSlice)
{
	const D3D12_RESOURCE_DESC desc = Resource->GetDesc();
	ASSERT(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, "Failed to create unordered access view: "
																	"D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS flag is not set for this texture.");

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = desc.Format;
	if (desc.DepthOrArraySize > 1)
	{
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
		uavDesc.Texture2DArray.MipSlice = mipSlice;
		uavDesc.Texture2DArray.FirstArraySlice = 0;
		uavDesc.Texture2DArray.ArraySize = desc.DepthOrArraySize;
	}
	else
	{
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Texture2D.MipSlice = mipSlice;
	}

	device->CreateUnorderedAccessView(Resource.Get(), nullptr, &uavDesc, Uav.CPUHandle);
}