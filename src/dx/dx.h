#pragma once

#include "pch.h"

const int NUM_FRAMES_IN_FLIGHT = 3;
const DXGI_FORMAT BACK_BUFFER_FORMAT = DXGI_FORMAT_R16G16B16A16_FLOAT;
const DXGI_FORMAT DEPTH_STENCIL_FORMAT = DXGI_FORMAT_D24_UNORM_S8_UINT;

const DXGI_FORMAT AMBIENT_MAP_FORMAT = DXGI_FORMAT_R16_UNORM;
const DXGI_FORMAT VELOCITY_BUFFER_FORMAT = DXGI_FORMAT_R16G16_FLOAT;

const DXGI_FORMAT GBUFFER_ALBEDO_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
const DXGI_FORMAT GBUFFER_NORMAL_FORMAT = DXGI_FORMAT_R16G16B16A16_FLOAT;
const DXGI_FORMAT GBUFFER_METALNESS_FORMAT = DXGI_FORMAT_R8_UNORM;
const DXGI_FORMAT GBUFFER_ROUGHNESS_FORMAT = DXGI_FORMAT_R8_UNORM;
const DXGI_FORMAT GBUFFER_AMBIENT_FORMAT = DXGI_FORMAT_R16G16B16A16_FLOAT;

typedef ComPtr<ID3D12Device> Device;
typedef ComPtr<IDXGIFactory4> Factory;
typedef ComPtr<IDXGIAdapter4> Adapter;
typedef ComPtr<IDXGISwapChain> SwapChain;
typedef ComPtr<ID3D12Resource> Resource;
typedef ComPtr<ID3DBlob> Blob;
typedef ComPtr<ID3D12PipelineState> PipelineState;
typedef ComPtr<ID3D12RootSignature> RootSignature;
typedef ComPtr<ID3D12CommandAllocator> CommandAllocator;
typedef ComPtr<ID3D12GraphicsCommandList2> GraphicsCommandList;
typedef ComPtr<ID3D12Fence> Fence;

#define SET_NAME(obj, name) ThrowIfFailed(obj->SetName(L##name));