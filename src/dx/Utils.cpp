#include "Utils.h"

static ComPtr<IDxcUtils> g_DxcUtils = nullptr;
static ComPtr<IDxcCompiler3> g_DxcCompiler = nullptr;
static ComPtr<IDxcIncludeHandler> g_DxcIncludeHandler = nullptr;

Shader Utils::CompileShader(const std::wstring &filename, const D3D_SHADER_MACRO *defines, const std::wstring &entrypoint, const std::wstring &target)
{
	if (!g_DxcUtils)
	{
		ThrowIfFailed(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&g_DxcUtils)));
		ThrowIfFailed(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&g_DxcCompiler)));
		ThrowIfFailed(g_DxcUtils->CreateDefaultIncludeHandler(&g_DxcIncludeHandler));
	}

	std::vector<LPCWSTR> compilationArguments = {
		L"-E", entrypoint.data(),
		L"-T", target.data(),
		L"-I", L"shaders/",
		DXC_ARG_DEBUG,
		DXC_ARG_WARNINGS_ARE_ERRORS};

	ComPtr<IDxcBlobEncoding> pSource = nullptr;
	g_DxcUtils->LoadFile(filename.data(), nullptr, &pSource);

	DxcBuffer source;
	source.Ptr = pSource->GetBufferPointer();
	source.Size = pSource->GetBufferSize();
	source.Encoding = DXC_CP_ACP;

	ComPtr<IDxcResult> result;
	const HRESULT hr = g_DxcCompiler->Compile(
		&source,
		compilationArguments.data(),
		(UINT32)compilationArguments.size(),
		g_DxcIncludeHandler.Get(),
		IID_PPV_ARGS(&result));

	ComPtr<IDxcBlobUtf8> errors = nullptr;
	result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);

	if (errors != nullptr && errors->GetStringLength() != 0)
		LOG_ERROR("Warnings and Errors: {}", errors->GetStringPointer());

	ThrowIfFailed(hr);

	Shader shader = nullptr;
	result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shader), nullptr);

	return shader;
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