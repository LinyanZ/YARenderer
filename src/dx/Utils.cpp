#include "Utils.h"
#include "dxcapi.h"

Blob Utils::CompileShader(const std::wstring &filename, const D3D_SHADER_MACRO *defines, const std::string &entrypoint, const std::string &target)
{
	ComPtr<IDxcUtils> pUtils;
	ComPtr<IDxcCompiler3> pCompiler;
	DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&pUtils));
	DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&pCompiler));

	UINT compileFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)
	compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	HRESULT hr = S_OK;

	Blob byteCode = nullptr;
	Blob errors;
	hr = D3DCompileFromFile(filename.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
							entrypoint.c_str(), target.c_str(), compileFlags, 0, &byteCode, &errors);

	if (errors != nullptr)
		OutputDebugStringA((char *)errors->GetBufferPointer());

	ThrowIfFailed(hr);

	return byteCode;
}

RootSignature Utils::CreateRootSignature(
	Device device,
	CD3DX12_ROOT_SIGNATURE_DESC &desc)
{
	RootSignature rootSignature;

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	Blob serializedRootSig = nullptr;
	Blob errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1,
											 serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char *)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(device->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature)));

	return rootSignature;
}

Resource Utils::CreateDefaultBuffer(Device device, GraphicsCommandList commandList, const void *initData, UINT64 byteSize, Resource &uploadBuffer)
{
	Resource defaultBuffer;

	// Create the actual default buffer resource.
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(byteSize),
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(defaultBuffer.GetAddressOf())));

	// In order to copy CPU memory data into our default buffer, we need to create
	// an intermediate upload heap.
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(byteSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(uploadBuffer.GetAddressOf())));

	// Describe the data we want to copy into the default buffer.
	D3D12_SUBRESOURCE_DATA subResourceData = {};
	subResourceData.pData = initData;
	subResourceData.RowPitch = byteSize;
	subResourceData.SlicePitch = subResourceData.RowPitch;

	// Schedule to copy the data to the default buffer resource.  At a high level, the helper function UpdateSubresources
	// will copy the CPU memory into the intermediate upload heap.  Then, using ID3D12CommandList::CopySubresourceRegion,
	// the intermediate upload heap data will be copied to mBuffer.
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
																		  D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
	UpdateSubresources<1>(commandList.Get(), defaultBuffer.Get(), uploadBuffer.Get(), 0, 0, 1, &subResourceData);
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
																		  D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));

	// Note: uploadBuffer has to be kept alive after the above function calls because
	// the command list has not been executed yet that performs the actual copy.
	// The caller can Release the uploadBuffer after it knows the copy has been executed.

	return defaultBuffer;
}