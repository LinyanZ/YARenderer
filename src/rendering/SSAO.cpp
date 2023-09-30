#include "pch.h"
#include "SSAO.h"
#include "dx/Utils.h"

SSAO::SSAO(Ref<DxContext> dxContext, UINT width, UINT height)
	: m_Device(dxContext->GetDevice()), m_RenderTargetWidth(width), m_RenderTargetHeight(height)
{
	BuildRootSignature();
	BuildPSOs();

	// Ambient occlusion maps are at half resolution.
	m_AmbientMap0 = Texture::Create(m_Device, width / 2, height / 2, 1, AMBIENT_MAP_FORMAT, 1);
	m_AmbientMap1 = Texture::Create(m_Device, width / 2, height / 2, 1, AMBIENT_MAP_FORMAT, 1);

	AllocateDescriptors(dxContext->GetRtvHeap(), dxContext->GetCbvSrvUavHeap());
	BuildDescriptors();

	m_Viewport = {0.0f, 0.0f, (float)m_RenderTargetWidth / 2, (float)m_RenderTargetHeight / 2, 0.0f, 1.0f};
	m_ScissorRect = {0, 0, (int)m_RenderTargetWidth / 2, (int)m_RenderTargetHeight / 2};
}

std::vector<float> SSAO::CalcGaussWeights(float sigma)
{
	float twoSigma2 = 2.0f * sigma * sigma;

	// Estimate the blur radius based on sigma since sigma controls the "width" of the bell curve.
	// For example, for sigma = 3, the width of the bell curve is
	int blurRadius = (int)ceil(2.0f * sigma);

	assert(blurRadius <= MaxBlurRadius);

	std::vector<float> weights;
	weights.resize(2 * blurRadius + 1);

	float weightSum = 0.0f;

	for (int i = -blurRadius; i <= blurRadius; ++i)
	{
		float x = (float)i;

		weights[i + blurRadius] = expf(-x * x / twoSigma2);

		weightSum += weights[i + blurRadius];
	}

	// Divide by the sum so all the weights add up to 1.0.
	for (int i = 0; i < weights.size(); ++i)
	{
		weights[i] /= weightSum;
	}

	return weights;
}

void SSAO::AllocateDescriptors(DescriptorHeap &GetRtvHeap, DescriptorHeap &SrvHeap)
{
	m_AmbientMap0.Srv = SrvHeap.Alloc();
	m_AmbientMap1.Srv = SrvHeap.Alloc();

	m_AmbientMap0.Rtv = GetRtvHeap.Alloc();
	m_AmbientMap1.Rtv = GetRtvHeap.Alloc();
}

void SSAO::BuildDescriptors()
{
	m_AmbientMap0.CreateSrv(m_Device, D3D12_SRV_DIMENSION_TEXTURE2D, 0, 1);
	m_AmbientMap1.CreateSrv(m_Device, D3D12_SRV_DIMENSION_TEXTURE2D, 0, 1);

	m_AmbientMap0.CreateRtv(m_Device, D3D12_RTV_DIMENSION_TEXTURE2D, 0, 0);
	m_AmbientMap1.CreateRtv(m_Device, D3D12_RTV_DIMENSION_TEXTURE2D, 0, 0);
}

void SSAO::OnResize(UINT width, UINT height)
{
	m_RenderTargetWidth = width;
	m_RenderTargetHeight = height;

	m_Viewport = {0.0f, 0.0f, (float)m_RenderTargetWidth / 2, (float)m_RenderTargetHeight / 2, 0.0f, 1.0f};
	m_ScissorRect = {0, 0, (int)m_RenderTargetWidth / 2, (int)m_RenderTargetHeight / 2};

	m_AmbientMap0.Resize(m_Device, width / 2, height / 2);
	m_AmbientMap1.Resize(m_Device, width / 2, height / 2);

	BuildDescriptors();
}

void SSAO::ComputeSSAO(GraphicsCommandList cmdList, Texture normalMap, Descriptor depthMapSrv, FrameResource *currFrame, int blurCount)
{
	cmdList->RSSetViewports(1, &m_Viewport);
	cmdList->RSSetScissorRects(1, &m_ScissorRect);

	// We compute the initial SSAO to AmbientMap0.

	// Change to RENDER_TARGET.
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_AmbientMap0.Resource.Get(),
																	  D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET));

	float clearValue[] = {1.0f, 1.0f, 1.0f, 1.0f};
	cmdList->ClearRenderTargetView(m_AmbientMap0.Rtv.CPUHandle, clearValue, 0, nullptr);

	// Specify the buffers we are going to render to.
	cmdList->OMSetRenderTargets(1, &m_AmbientMap0.Rtv.CPUHandle, true, nullptr);

	cmdList->SetGraphicsRootSignature(m_SSAORootSig.Get());
	cmdList->SetPipelineState(m_SSAOPso.Get());

	// Bind the constant buffer for this pass.
	auto ssaoCBAddress = currFrame->SSAOCB->GetResource()->GetGPUVirtualAddress();
	cmdList->SetGraphicsRootConstantBufferView(0, ssaoCBAddress);
	cmdList->SetGraphicsRoot32BitConstant(1, 0, 0);

	// Bind the normal and depth maps.
	cmdList->SetGraphicsRootDescriptorTable(2, normalMap.Srv.GPUHandle);
	cmdList->SetGraphicsRootDescriptorTable(3, depthMapSrv.GPUHandle);

	// Draw fullscreen quad.
	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);

	// Change back to GENERIC_READ so we can read the texture in a shader.
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_AmbientMap0.Resource.Get(),
																	  D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON));

	BlurAmbientMap(cmdList, normalMap, currFrame, blurCount);
}

void SSAO::BlurAmbientMap(GraphicsCommandList cmdList, Texture normalMap, FrameResource *currFrame, int blurCount)
{
	cmdList->SetPipelineState(m_BlurPso.Get());

	auto ssaoCBAddress = currFrame->SSAOCB->GetResource()->GetGPUVirtualAddress();
	cmdList->SetGraphicsRootConstantBufferView(0, ssaoCBAddress);

	for (int i = 0; i < blurCount; ++i)
	{
		BlurAmbientMap(cmdList, normalMap, true);
		BlurAmbientMap(cmdList, normalMap, false);
	}
}

void SSAO::BlurAmbientMap(GraphicsCommandList cmdList, Texture normalMap, bool horzBlur)
{
	ID3D12Resource *output = nullptr;
	CD3DX12_GPU_DESCRIPTOR_HANDLE inputSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE outputRtv;

	// Ping-pong the two ambient map textures as we apply
	// horizontal and vertical blur passes.
	if (horzBlur == true)
	{
		output = m_AmbientMap1.Resource.Get();
		inputSrv = m_AmbientMap0.Srv.GPUHandle;
		outputRtv = m_AmbientMap1.Rtv.CPUHandle;
		cmdList->SetGraphicsRoot32BitConstant(1, 1, 0);
	}
	else
	{
		output = m_AmbientMap0.Resource.Get();
		inputSrv = m_AmbientMap1.Srv.GPUHandle;
		outputRtv = m_AmbientMap0.Rtv.CPUHandle;
		cmdList->SetGraphicsRoot32BitConstant(1, 0, 0);
	}

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(output,
																	  D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET));

	float clearValue[] = {1.0f, 1.0f, 1.0f, 1.0f};
	cmdList->ClearRenderTargetView(outputRtv, clearValue, 0, nullptr);

	cmdList->OMSetRenderTargets(1, &outputRtv, true, nullptr);

	// Normal/depth map still bound.

	// Bind the normal and depth maps.
	// cmdList->SetGraphicsRootDescriptorTable(2, normalMap.Srv.GPUHandle);

	// Bind the input ambient map.
	cmdList->SetGraphicsRootDescriptorTable(4, inputSrv);

	// Draw fullscreen quad.
	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(output,
																	  D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON));
}

void SSAO::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE texTable0;
	texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

	CD3DX12_DESCRIPTOR_RANGE texTable1;
	texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);

	CD3DX12_DESCRIPTOR_RANGE texTable2;
	texTable2.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[5];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsConstantBufferView(0);
	slotRootParameter[1].InitAsConstants(1, 1);
	slotRootParameter[2].InitAsDescriptorTable(1, &texTable0, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[3].InitAsDescriptorTable(1, &texTable1, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[4].InitAsDescriptorTable(1, &texTable2, D3D12_SHADER_VISIBILITY_PIXEL);

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		0,								   // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT,	   // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC depthMapSam(
		1,								   // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,   // filter
		D3D12_TEXTURE_ADDRESS_MODE_BORDER, // addressU
		D3D12_TEXTURE_ADDRESS_MODE_BORDER, // addressV
		D3D12_TEXTURE_ADDRESS_MODE_BORDER, // addressW
		0.0f,
		0,
		D3D12_COMPARISON_FUNC_LESS_EQUAL,
		D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE);

	std::array<CD3DX12_STATIC_SAMPLER_DESC, 2> staticSamplers =
		{
			pointClamp, depthMapSam};

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(5, slotRootParameter,
											(UINT)staticSamplers.size(), staticSamplers.data(),
											D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	m_SSAORootSig = Utils::CreateRootSignature(m_Device, rootSigDesc);
}

void SSAO::BuildPSOs()
{
	Shader ssaoVSByteCode = Utils::CompileShader(L"Shaders\\ssao.hlsl", nullptr, L"VS", L"vs_6_6");
	Shader ssaoPSByteCode = Utils::CompileShader(L"Shaders\\ssao.hlsl", nullptr, L"PS", L"ps_6_6");

	Shader ssaoBlurVSByteCode = Utils::CompileShader(L"Shaders\\ssaoBlur.hlsl", nullptr, L"VS", L"vs_6_6");
	Shader ssaoBlurPSByteCode = Utils::CompileShader(L"Shaders\\ssaoBlur.hlsl", nullptr, L"PS", L"ps_6_6");

	//
	// PSO for SSAO.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
	desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	desc.SampleMask = UINT_MAX;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = AMBIENT_MAP_FORMAT;
	desc.SampleDesc = {1, 0};
	desc.InputLayout = {nullptr, 0};
	desc.pRootSignature = m_SSAORootSig.Get();
	desc.VS = CD3DX12_SHADER_BYTECODE(ssaoVSByteCode->GetBufferPointer(), ssaoVSByteCode->GetBufferSize());
	desc.PS = CD3DX12_SHADER_BYTECODE(ssaoPSByteCode->GetBufferPointer(), ssaoPSByteCode->GetBufferSize());

	// SSAO effect does not need the depth buffer.
	desc.DepthStencilState.DepthEnable = false;
	desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	desc.DSVFormat = DXGI_FORMAT_UNKNOWN;

	ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_SSAOPso)));

	//
	// PSO for SSAO blur.
	//
	desc.VS = CD3DX12_SHADER_BYTECODE(ssaoBlurVSByteCode->GetBufferPointer(), ssaoBlurVSByteCode->GetBufferSize());
	desc.PS = CD3DX12_SHADER_BYTECODE(ssaoBlurPSByteCode->GetBufferPointer(), ssaoBlurPSByteCode->GetBufferSize());

	ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_BlurPso)));
}