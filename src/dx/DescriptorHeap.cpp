#include "pch.h"
#include "DescriptorHeap.h"

DescriptorHeap DescriptorHeap::CreateDescriptorHeap(Device device, const D3D12_DESCRIPTOR_HEAP_DESC& desc)
{
	DescriptorHeap heap;
	ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap.Heap)));
	heap.Capacity = desc.NumDescriptors;
	heap.Size = 0;
	heap.DescriptorSize = device->GetDescriptorHandleIncrementSize(desc.Type);
	heap.IsShaderVisible = (desc.Type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	return heap;
}