#include "pch.h"
#include "RenderingUtils.h"
#include "dx/Utils.h"
#include "PipelineStates.h"

Texture RenderingUtils::ComputeDiffuseIrradianceCubemap(Ref<DxContext> dxContext, Texture &inputTex)
{
	LOG_INFO("Computing diffuse irradiance cubemp...");

	auto device = dxContext->GetDevice();
	auto commandList = dxContext->GetCommandList();
	auto &heap = dxContext->GetCbvSrvUavHeap();

	Texture outputTex = Texture::Create(device, 32, 32, 6, DXGI_FORMAT_R32G32B32A32_FLOAT, 1);
	outputTex.Srv = heap.Alloc();
	outputTex.CreateSrv(device, D3D12_SRV_DIMENSION_TEXTURECUBE, 0, 1);

	DescriptorHeapMark mark(heap);
	outputTex.Uav = heap.Alloc();
	outputTex.CreateUav(device, 0);

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(outputTex.Resource.Get(),
																		  D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

	ID3D12DescriptorHeap *descriptorHeaps[] = {heap.Heap.Get()};
	commandList->SetDescriptorHeaps(1, descriptorHeaps);

	commandList->SetPipelineState(PipelineStates::GetPSO("irmap"));
	commandList->SetComputeRootSignature(PipelineStates::GetRootSignature());

	UINT resources[] = {inputTex.Srv.Index, outputTex.Uav.Index};
	commandList->SetComputeRoot32BitConstants((UINT)RootParam::RenderResources, sizeof(resources) / sizeof(UINT), resources, 0);

	commandList->Dispatch(outputTex.Width / 32, outputTex.Height / 32, 6);
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(outputTex.Resource.Get(),
																		  D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON));

	dxContext->ExecuteCommandList();
	dxContext->Flush();

	return outputTex;
}

Texture RenderingUtils::ComputePrefilteredSpecularEnvironmentMap(Ref<DxContext> dxContext, Texture &inputTex)
{
	LOG_INFO("Computing pre-filtered specular environment map...");

	auto device = dxContext->GetDevice();
	auto commandList = dxContext->GetCommandList();
	auto &heap = dxContext->GetCbvSrvUavHeap();

	Texture outputTex = Texture::Create(device, 1024, 1024, 6, DXGI_FORMAT_R32G32B32A32_FLOAT);
	outputTex.Srv = heap.Alloc();
	outputTex.CreateSrv(device, D3D12_SRV_DIMENSION_TEXTURECUBE);

	DescriptorHeapMark mark(heap);

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
	ID3D12DescriptorHeap *descriptorHeaps[] = {heap.Heap.Get()};
	commandList->SetDescriptorHeaps(1, descriptorHeaps);

	commandList->SetPipelineState(PipelineStates::GetPSO("spmap"));
	commandList->SetComputeRootSignature(PipelineStates::GetRootSignature());

	const float deltaRoughness = 1.0f / std::max(float(outputTex.Levels - 1), 1.0f);
	for (UINT level = 1, size = 512; level < outputTex.Levels; level++, size /= 2)
	{
		UINT numGroups = std::max<UINT>(1, size / 32);
		float spmapRoughness = level * deltaRoughness;

		outputTex.Uav = heap.Alloc();
		outputTex.CreateUav(device, level);

		UINT resources[] = {inputTex.Srv.Index, outputTex.Uav.Index, *reinterpret_cast<UINT *>(&spmapRoughness)};
		commandList->SetComputeRoot32BitConstants((UINT)RootParam::RenderResources, sizeof(resources) / sizeof(UINT), resources, 0);

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
	auto &heap = dxContext->GetCbvSrvUavHeap();

	Texture outputTex = Texture::Create(device, 256, 256, 1, DXGI_FORMAT_R16G16_FLOAT, 1);
	outputTex.Srv = heap.Alloc();
	outputTex.CreateSrv(device, D3D12_SRV_DIMENSION_TEXTURE2D, 0, 1);

	DescriptorHeapMark mark(heap);
	outputTex.Uav = heap.Alloc();
	outputTex.CreateUav(device, 0);

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(outputTex.Resource.Get(),
																		  D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

	ID3D12DescriptorHeap *descriptorHeaps[] = {heap.Heap.Get()};
	commandList->SetDescriptorHeaps(1, descriptorHeaps);

	commandList->SetPipelineState(PipelineStates::GetPSO("spbrdf"));
	commandList->SetComputeRootSignature(PipelineStates::GetRootSignature());

	UINT resources[] = {outputTex.Uav.Index};
	commandList->SetComputeRoot32BitConstants((UINT)RootParam::RenderResources, sizeof(resources) / sizeof(UINT), resources, 0);

	commandList->Dispatch(outputTex.Width / 32, outputTex.Height / 32, 1);
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(outputTex.Resource.Get(),
																		  D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON));

	dxContext->ExecuteCommandList();
	dxContext->Flush();

	return outputTex;
}

Texture RenderingUtils::Equirect2Cubemap(Ref<DxContext> dxContext, Texture &inputTex)
{
	LOG_INFO("Converting equirect texture to cubemp...");

	auto commandList = dxContext->GetCommandList();
	auto device = dxContext->GetDevice();
	auto &cbvSrvUavHeap = dxContext->GetCbvSrvUavHeap();

	Texture outputTex = Texture::Create(device, 1024, 1024, 6, DXGI_FORMAT_R32G32B32A32_FLOAT);
	outputTex.Srv = cbvSrvUavHeap.Alloc();
	outputTex.CreateSrv(device, D3D12_SRV_DIMENSION_TEXTURECUBE);
	outputTex.Uav = cbvSrvUavHeap.Alloc();
	outputTex.CreateUav(device, 0);

	DescriptorHeapMark mark(cbvSrvUavHeap);

	inputTex.Srv = cbvSrvUavHeap.Alloc();
	inputTex.CreateSrv(device, D3D12_SRV_DIMENSION_TEXTURE2D, 0, 1);

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(outputTex.Resource.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

	ID3D12DescriptorHeap *descriptorHeaps[] = {cbvSrvUavHeap.Heap.Get()};
	commandList->SetDescriptorHeaps(1, descriptorHeaps);

	commandList->SetPipelineState(PipelineStates::GetPSO("equirect2Cube"));
	commandList->SetComputeRootSignature(PipelineStates::GetRootSignature());

	UINT resources[] = {inputTex.Srv.Index, outputTex.Uav.Index};
	commandList->SetComputeRoot32BitConstants((UINT)RootParam::RenderResources, sizeof(resources) / sizeof(UINT), resources, 0);

	commandList->Dispatch(outputTex.Width / 32, outputTex.Height / 32, 6);

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
										outputTex.Resource.Get(),
										D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON));

	dxContext->ExecuteCommandList();
	dxContext->Flush();

	return outputTex;
}

void RenderingUtils::GenerateMipmaps(Ref<DxContext> dxContext, Texture &texture)
{
	ASSERT(texture.Width == texture.Height, "Currently only support textures with equal width and height.");

	bool isPowerOfTwo = (texture.Width & (texture.Width - 1)) == 0;
	ASSERT(texture.Width != 0 && isPowerOfTwo, "Texture with size not power of two.");

	auto commandList = dxContext->GetCommandList();
	auto &cbvSrvUavHeap = dxContext->GetCbvSrvUavHeap();
	auto device = dxContext->GetDevice();

	DescriptorHeapMark mark(cbvSrvUavHeap);

	LOG_INFO("Generating mipmap for the environment map...");

	auto desc = texture.Resource->GetDesc();
	auto depth = desc.DepthOrArraySize;

	ID3D12DescriptorHeap *heaps[] = {cbvSrvUavHeap.Heap.Get()};
	commandList->SetDescriptorHeaps(1, heaps);

	commandList->SetPipelineState(PipelineStates::GetPSO("mipmap"));
	commandList->SetComputeRootSignature(PipelineStates::GetRootSignature());

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

		UINT resources[] = {tempTex.Srv.Index, tempTex.Uav.Index};
		commandList->SetComputeRoot32BitConstants((UINT)RootParam::RenderResources, sizeof(resources) / sizeof(UINT), resources, 0);

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