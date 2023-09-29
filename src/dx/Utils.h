#pragma once

#include "pch.h"
#include "dx/dx.h"

class Utils
{
public:
	static Blob CompileShader(
		const std::wstring& filename, const D3D_SHADER_MACRO* defines,
		const std::string& entrypoint, const std::string& target);

	static RootSignature CreateRootSignature(
		Device device, CD3DX12_ROOT_SIGNATURE_DESC& desc);

	static Resource CreateDefaultBuffer(
		Device device, GraphicsCommandList commandList,
		const void* initData, UINT64 byteSize, Resource& uploadBuffer);

	static UINT CalcConstantBufferByteSize(UINT byteSize)
	{
		// Constant buffers must be a multiple of the minimum hardware
		// allocation size (usually 256 bytes).  So round up to nearest
		// multiple of 256.  We do this by adding 255 and then masking off
		// the lower 2 bytes which store all bits < 256.
		// Example: Suppose byteSize = 300.
		// (300 + 255) & ~255
		// 555 & ~255
		// 0x022B & ~0x00ff
		// 0x022B & 0xff00
		// 0x0200
		// 512
		return (byteSize + 255) & ~255;
	}
};
