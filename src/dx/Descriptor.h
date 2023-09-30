#pragma once

#include "dx/dx.h"

struct Descriptor
{
	UINT Index;
	D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle;
};