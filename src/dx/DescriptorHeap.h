#pragma once

#include "dx/dx.h"
#include "Descriptor.h"

struct DescriptorHeap
{
	static DescriptorHeap CreateDescriptorHeap(Device device, const D3D12_DESCRIPTOR_HEAP_DESC &desc);

	ComPtr<ID3D12DescriptorHeap> Heap;
	UINT DescriptorSize;
	UINT Capacity;
	UINT Size;
	bool IsShaderVisible;

	ID3D12DescriptorHeap *Get() { return Heap.Get(); };
	ID3D12DescriptorHeap **GetAddressOf() { return Heap.GetAddressOf(); }

	Descriptor Alloc()
	{
		ASSERT(Size < Capacity, "Exceeding heap size.");
		Descriptor desc{Size, GetCPUHandle(Size), GetGPUHandle(Size)};
		Size++;
		return desc;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(UINT index) const
	{
		ASSERT(index < Capacity, "Index exceeding heap size.");
		return CD3DX12_CPU_DESCRIPTOR_HANDLE(
			Heap->GetCPUDescriptorHandleForHeapStart(),
			index, DescriptorSize);
	}

	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(UINT index) const
	{
		ASSERT(index < Capacity, "Index exceeding heap size.");
		if (!IsShaderVisible)
			return D3D12_GPU_DESCRIPTOR_HANDLE{0};
		return CD3DX12_GPU_DESCRIPTOR_HANDLE(
			Heap->GetGPUDescriptorHandleForHeapStart(),
			index, DescriptorSize);
	}
};

struct DescriptorHeapMark
{
	DescriptorHeapMark(DescriptorHeap &heap)
		: Heap(heap), Mark(heap.Size) {}

	~DescriptorHeapMark()
	{
		Heap.Size = Mark;
	}

	DescriptorHeap &Heap;
	const UINT Mark;
};
