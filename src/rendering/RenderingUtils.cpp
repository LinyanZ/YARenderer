#include "pch.h"
#include "RenderingUtils.h"
#include "dx/Utils.h"

std::unordered_map<std::string, RootSignature> RenderingUtils::m_RootSignatures;
std::unordered_map<std::string, PipelineState> RenderingUtils::m_PipelineStates;

void RenderingUtils::Init(Ref<DxContext> dxContext)
{
	Device device = dxContext->GetDevice();

	Blob equirect2CubeByteCode = Utils::CompileShader(L"shaders\\equirect2cube.hlsl", nullptr, "main", "cs_5_0");
	Blob irmapByteCode = Utils::CompileShader(L"shaders\\irmap.hlsl", nullptr, "main", "cs_5_0");
	Blob spmapByteCode = Utils::CompileShader(L"shaders\\spmap.hlsl", nullptr, "main", "cs_5_0");
	Blob spbrdfByteCode = Utils::CompileShader(L"shaders\\spbrdf.hlsl", nullptr, "main", "cs_5_0");
	Blob mipmapByteCode = Utils::CompileShader(L"shaders\\downsample_array.hlsl", nullptr, "downsample_linear", "cs_5_0");

	// create universal compute shader root signature
	{
		CD3DX12_DESCRIPTOR_RANGE srvTable;
		srvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
		CD3DX12_DESCRIPTOR_RANGE uavTable;
		uavTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);

		CD3DX12_ROOT_PARAMETER slotRootParameter[3];
		slotRootParameter[0].InitAsDescriptorTable(1, &srvTable);
		slotRootParameter[1].InitAsDescriptorTable(1, &uavTable);
		slotRootParameter[2].InitAsConstants(1, 0);

		CD3DX12_STATIC_SAMPLER_DESC computeSamplerDesc{ 0, D3D12_FILTER_MIN_MAG_MIP_LINEAR };

		CD3DX12_ROOT_SIGNATURE_DESC desc(3, slotRootParameter,
			1, &computeSamplerDesc,
			D3D12_ROOT_SIGNATURE_FLAG_NONE);

		m_RootSignatures["compute"] = Utils::CreateRootSignature(device, desc);
	}

	// mimap generation
	{
		CD3DX12_DESCRIPTOR_RANGE srvTable;
		srvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
		CD3DX12_DESCRIPTOR_RANGE uavTable;
		uavTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);

		CD3DX12_ROOT_PARAMETER slotRootParameter[2];
		slotRootParameter[0].InitAsDescriptorTable(1, &srvTable);
		slotRootParameter[1].InitAsDescriptorTable(1, &uavTable);

		CD3DX12_ROOT_SIGNATURE_DESC desc(2, slotRootParameter,
			0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

		m_RootSignatures["mipmap"] = Utils::CreateRootSignature(device, desc);
	}

	// create PSOs
	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = m_RootSignatures["compute"].Get();

	// equirect to cubemap
	psoDesc.CS = CD3DX12_SHADER_BYTECODE(equirect2CubeByteCode.Get());
	ThrowIfFailed(device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&m_PipelineStates["equirect2Cube"])));

	// diffuse irradiance cubemap
	psoDesc.CS = CD3DX12_SHADER_BYTECODE(irmapByteCode.Get());
	ThrowIfFailed(device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&m_PipelineStates["irmap"])));

	// pre-filtered specular environment map
	psoDesc.CS = CD3DX12_SHADER_BYTECODE(spmapByteCode.Get());
	ThrowIfFailed(device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&m_PipelineStates["spmap"])));

	// BRDF 2D LUT
	psoDesc.CS = CD3DX12_SHADER_BYTECODE(spbrdfByteCode.Get());
	ThrowIfFailed(device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&m_PipelineStates["spbrdf"])));

	// mipmap
	psoDesc.pRootSignature = m_RootSignatures["mipmap"].Get();
	psoDesc.CS = CD3DX12_SHADER_BYTECODE(mipmapByteCode.Get());
	ThrowIfFailed(device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&m_PipelineStates["mipmap"])));
}

void RenderingUtils::Cleanup()
{
	m_RootSignatures.clear();
	m_PipelineStates.clear();
}

Texture RenderingUtils::ComputeDiffuseIrradianceCubemap(Ref<DxContext> dxContext, Texture& inputTex)
{
	LOG_INFO("Computing diffuse irradiance cubemp...");

	auto device = dxContext->GetDevice();
	auto commandList = dxContext->GetCommandList();
	auto& heap = dxContext->GetCbvSrvUavHeap();

	Texture outputTex = Texture::Create(device, 32, 32, 6, DXGI_FORMAT_R32G32B32A32_FLOAT, 1);
	outputTex.Srv = heap.Alloc();
	outputTex.CreateSrv(device, D3D12_SRV_DIMENSION_TEXTURECUBE, 0, 1);

	DxDescriptorHeapMark mark(heap);
	outputTex.Uav = heap.Alloc();
	outputTex.CreateUav(device, 0);

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(outputTex.Resource.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

	ID3D12DescriptorHeap* descriptorHeaps[] = { heap.Heap.Get() };
	commandList->SetDescriptorHeaps(1, descriptorHeaps);

	commandList->SetPipelineState(m_PipelineStates["irmap"].Get());
	commandList->SetComputeRootSignature(m_RootSignatures["compute"].Get());
	commandList->SetComputeRootDescriptorTable(0, inputTex.Srv.GPUHandle);
	commandList->SetComputeRootDescriptorTable(1, outputTex.Uav.GPUHandle);

	commandList->Dispatch(outputTex.Width / 32, outputTex.Height / 32, 6);
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(outputTex.Resource.Get(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON));

	dxContext->ExecuteCommandList();
	dxContext->Flush();

	return outputTex;
}

Texture RenderingUtils::ComputePrefilteredSpecularEnvironmentMap(Ref<DxContext> dxContext, Texture& inputTex)
{
	LOG_INFO("Computing pre-filtered specular environment map...");

	auto device = dxContext->GetDevice();
	auto commandList = dxContext->GetCommandList();
	auto& heap = dxContext->GetCbvSrvUavHeap();

	Texture outputTex = Texture::Create(device, 1024, 1024, 6, DXGI_FORMAT_R32G32B32A32_FLOAT);
	outputTex.Srv = heap.Alloc();
	outputTex.CreateSrv(device, D3D12_SRV_DIMENSION_TEXTURECUBE);

	DxDescriptorHeapMark mark(heap);

	// copy 0th mipmap level into specular environment map
	D3D12_RESOURCE_BARRIER preCopyBarriers[] =
	{
		CD3DX12_RESOURCE_BARRIER::Transition(outputTex.Resource.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST),
		CD3DX12_RESOURCE_BARRIER::Transition(inputTex.Resource.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_SOURCE),
	};
	D3D12_RESOURCE_BARRIER postCopyBarriers[] =
	{
		CD3DX12_RESOURCE_BARRIER::Transition(outputTex.Resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
		CD3DX12_RESOURCE_BARRIER::Transition(inputTex.Resource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON),
	};

	commandList->ResourceBarrier(2, preCopyBarriers);
	for (UINT arraySlice = 0; arraySlice < 6; arraySlice++)
	{
		auto subresourceIndex = D3D12CalcSubresource(0, arraySlice, 0, outputTex.Levels, 6);
		commandList->CopyTextureRegion(
			&CD3DX12_TEXTURE_COPY_LOCATION(outputTex.Resource.Get(), subresourceIndex),
			0, 0, 0,
			&CD3DX12_TEXTURE_COPY_LOCATION(inputTex.Resource.Get(), subresourceIndex),
			nullptr);
	}
	commandList->ResourceBarrier(2, postCopyBarriers);

	// pre-filter rest of the mip chain
	ID3D12DescriptorHeap* descriptorHeaps[] = { heap.Heap.Get() };
	commandList->SetDescriptorHeaps(1, descriptorHeaps);

	commandList->SetPipelineState(m_PipelineStates["spmap"].Get());
	commandList->SetComputeRootSignature(m_RootSignatures["compute"].Get());
	commandList->SetComputeRootDescriptorTable(0, inputTex.Srv.GPUHandle);

	const float deltaRoughness = 1.0f / std::max(float(outputTex.Levels - 1), 1.0f);
	for (UINT level = 1, size = 512; level < outputTex.Levels; level++, size /= 2)
	{
		UINT numGroups = std::max<UINT>(1, size / 32);
		float spmapRoughness = level * deltaRoughness;

		outputTex.Uav = heap.Alloc();
		outputTex.CreateUav(device, level);

		commandList->SetComputeRootDescriptorTable(1, outputTex.Uav.GPUHandle);
		commandList->SetComputeRoot32BitConstants(2, 1, &spmapRoughness, 0);
		commandList->Dispatch(numGroups, numGroups, 6);
	}
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(outputTex.Resource.Get(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON));

	dxContext->ExecuteCommandList();
	dxContext->Flush();

	return outputTex;
}

Texture RenderingUtils::ComputeBRDFLookUpTable(Ref<DxContext> dxContext)
{
	LOG_INFO("Computing BRDF 2D LUT for split-sum approximation...");

	auto device = dxContext->GetDevice();
	auto commandList = dxContext->GetCommandList();
	auto& heap = dxContext->GetCbvSrvUavHeap();

	Texture outputTex = Texture::Create(device, 256, 256, 1, DXGI_FORMAT_R16G16_FLOAT, 1);
	outputTex.Srv = heap.Alloc();
	outputTex.CreateSrv(device, D3D12_SRV_DIMENSION_TEXTURE2D, 0, 1);

	DxDescriptorHeapMark mark(heap);
	outputTex.Uav = heap.Alloc();
	outputTex.CreateUav(device, 0);

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(outputTex.Resource.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

	ID3D12DescriptorHeap* descriptorHeaps[] = { heap.Heap.Get() };
	commandList->SetDescriptorHeaps(1, descriptorHeaps);

	commandList->SetPipelineState(m_PipelineStates["spbrdf"].Get());
	commandList->SetComputeRootSignature(m_RootSignatures["compute"].Get());
	commandList->SetComputeRootDescriptorTable(1, outputTex.Uav.GPUHandle);

	commandList->Dispatch(outputTex.Width / 32, outputTex.Height / 32, 1);
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(outputTex.Resource.Get(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON));

	dxContext->ExecuteCommandList();
	dxContext->Flush();

	return outputTex;
}

Texture RenderingUtils::Equirect2Cubemap(Ref<DxContext> dxContext, Texture& inputTex)
{
	LOG_INFO("Converting equirect texture to cubemp...");

	auto commandList = dxContext->GetCommandList();
	auto device = dxContext->GetDevice();
	auto& cbvSrvUavHeap = dxContext->GetCbvSrvUavHeap();

	Texture outputTex = Texture::Create(device, 1024, 1024, 6, DXGI_FORMAT_R32G32B32A32_FLOAT);
	outputTex.Srv = cbvSrvUavHeap.Alloc();
	outputTex.CreateSrv(device, D3D12_SRV_DIMENSION_TEXTURECUBE);
	outputTex.Uav = cbvSrvUavHeap.Alloc();
	outputTex.CreateUav(device, 0);

	DxDescriptorHeapMark mark(cbvSrvUavHeap);

	inputTex.Srv = cbvSrvUavHeap.Alloc();
	inputTex.CreateSrv(device, D3D12_SRV_DIMENSION_TEXTURE2D, 0, 1);

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(outputTex.Resource.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

	ID3D12DescriptorHeap* descriptorHeaps[] = { cbvSrvUavHeap.Heap.Get() };
	commandList->SetDescriptorHeaps(1, descriptorHeaps);

	commandList->SetComputeRootSignature(m_RootSignatures["compute"].Get());
	commandList->SetPipelineState(m_PipelineStates["equirect2Cube"].Get());

	commandList->SetComputeRootDescriptorTable(0, inputTex.Srv.GPUHandle);
	commandList->SetComputeRootDescriptorTable(1, outputTex.Uav.GPUHandle);

	commandList->Dispatch(outputTex.Width / 32, outputTex.Height / 32, 6);

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		outputTex.Resource.Get(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON));

	dxContext->ExecuteCommandList();
	dxContext->Flush();

	return outputTex;
}

void RenderingUtils::GenerateMipmaps(Ref<DxContext> dxContext, Texture& texture)
{
	ASSERT(texture.Width == texture.Height, "Currently only support textures with equal width and height.");

	bool isPowerOfTwo = (texture.Width & (texture.Width - 1)) == 0;
	ASSERT(texture.Width != 0 && isPowerOfTwo, "Texture with size not power of two.");

	auto commandList = dxContext->GetCommandList();
	auto& cbvSrvUavHeap = dxContext->GetCbvSrvUavHeap();
	auto device = dxContext->GetDevice();

	DxDescriptorHeapMark mark(cbvSrvUavHeap);

	LOG_INFO("Generating mipmap for the environment map...");

	auto desc = texture.Resource->GetDesc();
	auto depth = desc.DepthOrArraySize;

	ID3D12PipelineState* pipelineState = nullptr;

	ID3D12DescriptorHeap* heaps[] = { cbvSrvUavHeap.Heap.Get() };
	commandList->SetDescriptorHeaps(1, heaps);
	commandList->SetComputeRootSignature(m_RootSignatures["mipmap"].Get());
	commandList->SetPipelineState(m_PipelineStates["mipmap"].Get());

	std::vector<CD3DX12_RESOURCE_BARRIER> preDispatchBarriers(depth);
	std::vector<CD3DX12_RESOURCE_BARRIER> postDispatchBarriers(depth);

	// point to the same ID3D12Resource, but with different srv and uav
	Texture tempTex = texture;

	for (UINT level = 1, levelWidth = texture.Width / 2; level < texture.Levels; level++, levelWidth /= 2)
	{
		tempTex.Srv = cbvSrvUavHeap.Alloc();
		tempTex.Uav = cbvSrvUavHeap.Alloc();
		tempTex.CreateSrv(device,
			depth > 1 ? D3D12_SRV_DIMENSION_TEXTURE2DARRAY : D3D12_SRV_DIMENSION_TEXTURE2D, level - 1, 1);
		tempTex.CreateUav(device, level);

		for (UINT arraySlice = 0; arraySlice < depth; arraySlice++)
		{
			auto subresourceIndex = D3D12CalcSubresource(1, arraySlice, 0, texture.Levels, depth);
			preDispatchBarriers[arraySlice] = CD3DX12_RESOURCE_BARRIER::Transition(texture.Resource.Get(),
				D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, subresourceIndex);
			postDispatchBarriers[arraySlice] = CD3DX12_RESOURCE_BARRIER::Transition(texture.Resource.Get(),
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON, subresourceIndex);
		}

		commandList->ResourceBarrier(depth, preDispatchBarriers.data());

		commandList->SetComputeRootDescriptorTable(0, tempTex.Srv.GPUHandle);
		commandList->SetComputeRootDescriptorTable(1, tempTex.Uav.GPUHandle);

		UINT threadGroupCount = std::max<UINT>(levelWidth / 8, 1);
		commandList->Dispatch(threadGroupCount, threadGroupCount, depth);

		commandList->ResourceBarrier(depth, postDispatchBarriers.data());
	}

	dxContext->ExecuteCommandList();
	dxContext->Flush();
}

float RenderingUtils::Halton(UINT i, UINT b)
{
	float f = 1.0f;
	float r = 0.0f;

	while (i > 0)
	{
		f /= static_cast<float>(b);
		r = r + f * static_cast<float>(i % b);
		i = static_cast<UINT>(floorf(static_cast<float>(i) / static_cast<float>(b)));
	}

	return r;
}