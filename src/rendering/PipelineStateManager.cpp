#include "pch.h"
#include "PipelineStateManager.h"
#include "Renderer.h"
#include "Ssao.h"
#include "dx/Utils.h"

PipelineStateManager::PipelineStateManager(Device device)
	: m_Device(device)
{
	BuildShadersAndInputLayouts();
	BuildRootSignatures();
	BuildPSOs();
}

void PipelineStateManager::BuildShadersAndInputLayouts()
{
	m_VSByteCodes["default"] = Utils::CompileShader(L"shaders\\defaultVS.hlsl", nullptr, "VS", "vs_5_0");

	m_PSByteCodes["drawNormal"] = Utils::CompileShader(L"shaders\\drawNormal.hlsl", nullptr, "PS", "ps_5_0");
	m_PSByteCodes["forwardRendering"] = Utils::CompileShader(L"shaders\\forwardRendering.hlsl", nullptr, "PS", "ps_5_0");

	m_VSByteCodes["shadow"] = Utils::CompileShader(L"shaders\\shadow.hlsl", nullptr, "VS", "vs_5_0");
	m_PSByteCodes["shadow"] = Utils::CompileShader(L"shaders\\shadow.hlsl", nullptr, "PS", "ps_5_0");

	m_VSByteCodes["velocityBuffer"] = Utils::CompileShader(L"shaders\\velocityBuffer.hlsl", nullptr, "VS", "vs_5_0");
	m_PSByteCodes["velocityBuffer"] = Utils::CompileShader(L"shaders\\velocityBuffer.hlsl", nullptr, "PS", "ps_5_0");

	m_PSByteCodes["gbuffer"] = Utils::CompileShader(L"shaders\\gbuffer.hlsl", nullptr, "PS", "ps_5_0");

	m_VSByteCodes["lightingPass"] = Utils::CompileShader(L"shaders\\lightingPassVS.hlsl", nullptr, "VS", "vs_5_0");
	m_PSByteCodes["lightingPass"] = Utils::CompileShader(L"shaders\\lightingPassPS.hlsl", nullptr, "PS", "ps_5_0");

	m_VSByteCodes["deferredAmbientLight"] = Utils::CompileShader(L"shaders\\deferredAmbientLight.hlsl", nullptr, "VS", "vs_5_0");
	m_PSByteCodes["deferredAmbientLight"] = Utils::CompileShader(L"shaders\\deferredAmbientLight.hlsl", nullptr, "PS", "ps_5_0");

	m_VSByteCodes["debug"] = Utils::CompileShader(L"shaders\\debug.hlsl", nullptr, "VS", "vs_5_0");
	m_PSByteCodes["debug"] = Utils::CompileShader(L"shaders\\debug.hlsl", nullptr, "PS", "ps_5_0");

	m_VSByteCodes["skybox"] = Utils::CompileShader(L"shaders\\skybox.hlsl", nullptr, "VS", "vs_5_0");
	m_PSByteCodes["skybox"] = Utils::CompileShader(L"shaders\\skybox.hlsl", nullptr, "PS", "ps_5_0");

	m_InputLayouts["default"] =
		{
			{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{"BITANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 48, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		};

	m_InputLayouts["skybox"] =
		{
			{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
}

void PipelineStateManager::BuildRootSignatures()
{
	// draw normal
	{
		CD3DX12_DESCRIPTOR_RANGE descriptorRanges[] =
			{
				{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0}, // normal
			};

		CD3DX12_ROOT_PARAMETER slotRootParameter[4];
		slotRootParameter[0].InitAsConstantBufferView(0);
		slotRootParameter[1].InitAsConstantBufferView(1);
		slotRootParameter[2].InitAsConstantBufferView(2);
		slotRootParameter[3].InitAsDescriptorTable(1, &descriptorRanges[0], D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_STATIC_SAMPLER_DESC defaultSamplerDesc{0, D3D12_FILTER_ANISOTROPIC};
		defaultSamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		CD3DX12_ROOT_SIGNATURE_DESC desc(4, slotRootParameter, 1, &defaultSamplerDesc,
										 D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		m_RootSignatures["drawNormal"] = Utils::CreateRootSignature(m_Device, desc);
	}

	// forward rendering root signature
	{
		CD3DX12_DESCRIPTOR_RANGE descriptorRanges[] =
			{
				{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0},	 // albedo
				{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0},	 // normal
				{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0},	 // metalness
				{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3, 0},	 // roughness
				{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 4, 0},	 // irmap, spmap, lut
				{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 7, 0},	 // ssao
				{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 8, 0},	 // shadow map
				{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 10, 0}, // vxgi
			};

		CD3DX12_ROOT_PARAMETER slotRootParameter[13];
		slotRootParameter[0].InitAsConstantBufferView(0);
		slotRootParameter[1].InitAsConstantBufferView(1);
		slotRootParameter[2].InitAsConstantBufferView(2);
		slotRootParameter[3].InitAsDescriptorTable(1, &descriptorRanges[0], D3D12_SHADER_VISIBILITY_PIXEL);
		slotRootParameter[4].InitAsDescriptorTable(1, &descriptorRanges[1], D3D12_SHADER_VISIBILITY_PIXEL);
		slotRootParameter[5].InitAsDescriptorTable(1, &descriptorRanges[2], D3D12_SHADER_VISIBILITY_PIXEL);
		slotRootParameter[6].InitAsDescriptorTable(1, &descriptorRanges[3], D3D12_SHADER_VISIBILITY_PIXEL);
		slotRootParameter[7].InitAsDescriptorTable(1, &descriptorRanges[4], D3D12_SHADER_VISIBILITY_PIXEL);
		slotRootParameter[8].InitAsDescriptorTable(1, &descriptorRanges[5], D3D12_SHADER_VISIBILITY_PIXEL);
		slotRootParameter[9].InitAsDescriptorTable(1, &descriptorRanges[6], D3D12_SHADER_VISIBILITY_PIXEL);
		slotRootParameter[10].InitAsShaderResourceView(9);
		slotRootParameter[11].InitAsConstantBufferView(3);
		slotRootParameter[12].InitAsDescriptorTable(1, &descriptorRanges[7], D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_STATIC_SAMPLER_DESC defaultSamplerDesc{0, D3D12_FILTER_ANISOTROPIC};
		defaultSamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		CD3DX12_STATIC_SAMPLER_DESC spBRDF_SamplerDesc{1, D3D12_FILTER_MIN_MAG_MIP_LINEAR};
		spBRDF_SamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		spBRDF_SamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		spBRDF_SamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		CD3DX12_STATIC_SAMPLER_DESC shadow(
			2,										   // shaderRegister
			D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT, // filter
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,		   // addressU
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,		   // addressV
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,		   // addressW
			0.0f,									   // mipLODBias
			16,										   // maxAnisotropy
			D3D12_COMPARISON_FUNC_LESS_EQUAL,
			D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK);
		CD3DX12_STATIC_SAMPLER_DESC pointSamplerDesc(
			3,								  // shaderRegister
			D3D12_FILTER_MIN_MAG_MIP_POINT,	  // filter
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP, // addressU
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP  // addressV
		);

		D3D12_STATIC_SAMPLER_DESC samplers[4] = {defaultSamplerDesc, spBRDF_SamplerDesc, shadow, pointSamplerDesc};

		CD3DX12_ROOT_SIGNATURE_DESC desc(13, slotRootParameter, 4, samplers,
										 D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		m_RootSignatures["forwardRendering"] = Utils::CreateRootSignature(m_Device, desc);
	}

	// cascade shadow map root signature
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[3];
		slotRootParameter[0].InitAsConstantBufferView(0); // object cbuffer
		slotRootParameter[1].InitAsConstantBufferView(1); // shadow pass cbuffer
		slotRootParameter[2].InitAsConstants(1, 2);		  // cascade shadow index

		CD3DX12_ROOT_SIGNATURE_DESC desc(3, slotRootParameter, 0, nullptr,
										 D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		m_RootSignatures["shadow"] = Utils::CreateRootSignature(m_Device, desc);
	}

	// defered rendering g-buffer pass root signature
	{
		CD3DX12_DESCRIPTOR_RANGE descriptorRanges[] =
			{
				{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0}, // albedo
				{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0}, // normal
				{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0}, // metalness
				{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3, 0}, // roughness
				{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 4, 0}, // irmap, spmap, lut
			};

		CD3DX12_ROOT_PARAMETER slotRootParameter[8];
		slotRootParameter[0].InitAsConstantBufferView(0);
		slotRootParameter[1].InitAsConstantBufferView(1);
		slotRootParameter[2].InitAsConstantBufferView(2);
		slotRootParameter[3].InitAsDescriptorTable(1, &descriptorRanges[0], D3D12_SHADER_VISIBILITY_PIXEL);
		slotRootParameter[4].InitAsDescriptorTable(1, &descriptorRanges[1], D3D12_SHADER_VISIBILITY_PIXEL);
		slotRootParameter[5].InitAsDescriptorTable(1, &descriptorRanges[2], D3D12_SHADER_VISIBILITY_PIXEL);
		slotRootParameter[6].InitAsDescriptorTable(1, &descriptorRanges[3], D3D12_SHADER_VISIBILITY_PIXEL);
		slotRootParameter[7].InitAsDescriptorTable(1, &descriptorRanges[4], D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_STATIC_SAMPLER_DESC defaultSamplerDesc{0, D3D12_FILTER_ANISOTROPIC};
		defaultSamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		CD3DX12_STATIC_SAMPLER_DESC spBRDF_SamplerDesc{1, D3D12_FILTER_MIN_MAG_MIP_LINEAR};
		spBRDF_SamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		spBRDF_SamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		spBRDF_SamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		D3D12_STATIC_SAMPLER_DESC samplers[2] = {defaultSamplerDesc, spBRDF_SamplerDesc};

		CD3DX12_ROOT_SIGNATURE_DESC desc(8, slotRootParameter, 2, samplers,
										 D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		m_RootSignatures["gbuffer"] = Utils::CreateRootSignature(m_Device, desc);
	}

	// defered rendering lighting pass 0 root signature
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[3];
		slotRootParameter[0].InitAsConstants(1, 0);
		slotRootParameter[1].InitAsConstantBufferView(1);
		slotRootParameter[2].InitAsShaderResourceView(0);

		CD3DX12_ROOT_SIGNATURE_DESC desc(3, slotRootParameter, 0, nullptr,
										 D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		m_RootSignatures["deferredRenderingLightingPass0"] = Utils::CreateRootSignature(m_Device, desc);
	}

	// defered rendering ambient light pass
	{
		CD3DX12_DESCRIPTOR_RANGE descriptorRanges[] =
			{
				{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0}, // albedo, normal, metalness, roughness
				{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0}, // depth
			};

		CD3DX12_ROOT_PARAMETER slotRootParameter[2];
		slotRootParameter[0].InitAsDescriptorTable(1, &descriptorRanges[0]);
		slotRootParameter[1].InitAsDescriptorTable(1, &descriptorRanges[1]);

		CD3DX12_STATIC_SAMPLER_DESC defaultSamplerDesc{0, D3D12_FILTER_ANISOTROPIC};
		defaultSamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		CD3DX12_ROOT_SIGNATURE_DESC desc(2, slotRootParameter, 1, &defaultSamplerDesc,
										 D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		m_RootSignatures["deferredAmbientLight"] = Utils::CreateRootSignature(m_Device, desc);
	}

	// defered rendering lighting pass 1 root signature
	{
		CD3DX12_DESCRIPTOR_RANGE descriptorRanges[] =
			{
				{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 1, 0}, // albedo, normal, metalness, roughness
				{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5, 0}, // depth
			};

		CD3DX12_ROOT_PARAMETER slotRootParameter[5];
		slotRootParameter[0].InitAsConstants(1, 0);
		slotRootParameter[1].InitAsConstantBufferView(1);
		slotRootParameter[2].InitAsShaderResourceView(0);
		slotRootParameter[3].InitAsDescriptorTable(1, &descriptorRanges[0]);
		slotRootParameter[4].InitAsDescriptorTable(1, &descriptorRanges[1]);

		CD3DX12_ROOT_SIGNATURE_DESC desc(5, slotRootParameter, 0, nullptr,
										 D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		m_RootSignatures["deferredRenderingLightingPass1"] = Utils::CreateRootSignature(m_Device, desc);
	}

	// skybox root signature
	{
		CD3DX12_DESCRIPTOR_RANGE texTable;
		texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

		CD3DX12_ROOT_PARAMETER slotRootParameter[3];
		slotRootParameter[0].InitAsConstantBufferView(0);										 // pass constant buffer
		slotRootParameter[1].InitAsConstantBufferView(1);										 // pass constant buffer
		slotRootParameter[2].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL); // cube map texture

		auto staticSamplers = GetStaticSamplers();

		CD3DX12_ROOT_SIGNATURE_DESC desc(3, slotRootParameter,
										 1, staticSamplers.data(),
										 D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		m_RootSignatures["skybox"] = Utils::CreateRootSignature(m_Device, desc);
	}

	// velocity buffer
	{
		CD3DX12_ROOT_PARAMETER slotRootParameter[3];
		slotRootParameter[0].InitAsConstantBufferView(0);
		slotRootParameter[1].InitAsConstantBufferView(1);
		slotRootParameter[2].InitAsConstantBufferView(2);

		CD3DX12_ROOT_SIGNATURE_DESC desc(3, slotRootParameter,
										 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		m_RootSignatures["velocityBuffer"] = Utils::CreateRootSignature(m_Device, desc);
	}

	// debug
	{
		CD3DX12_DESCRIPTOR_RANGE descriptorRange{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0};

		CD3DX12_ROOT_PARAMETER slotRootParameter[2];
		slotRootParameter[0].InitAsDescriptorTable(1, &descriptorRange);
		slotRootParameter[1].InitAsConstants(1, 0);

		CD3DX12_STATIC_SAMPLER_DESC sampler{0, D3D12_FILTER_MIN_MAG_MIP_POINT};
		sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		CD3DX12_ROOT_SIGNATURE_DESC desc(2, slotRootParameter, 1, &sampler,
										 D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		m_RootSignatures["debug"] = Utils::CreateRootSignature(m_Device, desc);
	}

	// voxelizer
	{
		CD3DX12_DESCRIPTOR_RANGE descriptorRanges[] =
			{
				{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0}, // albedo
				{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0}, // normal
				{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0}, // metalness
				{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3, 0}, // roughness
				{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4, 0}, // shadow map
				{D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0}, // volume texture albedo
			};

		CD3DX12_ROOT_PARAMETER slotRootParameter[11];
		slotRootParameter[0].InitAsConstantBufferView(0); // per object cb
		slotRootParameter[1].InitAsConstantBufferView(1); // per pass cb
		slotRootParameter[2].InitAsConstantBufferView(2); // material cb
		slotRootParameter[3].InitAsDescriptorTable(1, &descriptorRanges[0], D3D12_SHADER_VISIBILITY_PIXEL);
		slotRootParameter[4].InitAsDescriptorTable(1, &descriptorRanges[1], D3D12_SHADER_VISIBILITY_PIXEL);
		slotRootParameter[5].InitAsDescriptorTable(1, &descriptorRanges[2], D3D12_SHADER_VISIBILITY_PIXEL);
		slotRootParameter[6].InitAsDescriptorTable(1, &descriptorRanges[3], D3D12_SHADER_VISIBILITY_PIXEL);
		slotRootParameter[7].InitAsDescriptorTable(1, &descriptorRanges[4], D3D12_SHADER_VISIBILITY_PIXEL);
		slotRootParameter[8].InitAsShaderResourceView(5); // light data
		slotRootParameter[9].InitAsConstantBufferView(3); // shadow cb
		slotRootParameter[10].InitAsDescriptorTable(1, &descriptorRanges[5], D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_STATIC_SAMPLER_DESC defaultSamplerDesc{0, D3D12_FILTER_ANISOTROPIC};
		defaultSamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		CD3DX12_STATIC_SAMPLER_DESC spBRDF_SamplerDesc{1, D3D12_FILTER_MIN_MAG_MIP_LINEAR};
		spBRDF_SamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		spBRDF_SamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		spBRDF_SamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		CD3DX12_STATIC_SAMPLER_DESC shadow(
			2,										   // shaderRegister
			D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT, // filter
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,		   // addressU
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,		   // addressV
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,		   // addressW
			0.0f,									   // mipLODBias
			16,										   // maxAnisotropy
			D3D12_COMPARISON_FUNC_LESS_EQUAL,
			D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK);
		CD3DX12_STATIC_SAMPLER_DESC pointSamplerDesc(
			3,								  // shaderRegister
			D3D12_FILTER_MIN_MAG_MIP_POINT,	  // filter
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP, // addressU
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP  // addressV
		);

		D3D12_STATIC_SAMPLER_DESC samplers[4] = {defaultSamplerDesc, spBRDF_SamplerDesc, shadow, pointSamplerDesc};

		CD3DX12_ROOT_SIGNATURE_DESC desc(11, slotRootParameter, 4, samplers,
										 D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		m_RootSignatures["voxelize"] = Utils::CreateRootSignature(m_Device, desc);
	}

	// debug voxel
	{
		CD3DX12_DESCRIPTOR_RANGE descriptorRanges[] =
			{
				{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0}, // volume texture albedo
			};

		CD3DX12_ROOT_PARAMETER slotRootParameter[3];
		slotRootParameter[0].InitAsConstantBufferView(0);
		slotRootParameter[1].InitAsDescriptorTable(1, &descriptorRanges[0], D3D12_SHADER_VISIBILITY_ALL);
		slotRootParameter[2].InitAsConstants(1, 1);

		CD3DX12_STATIC_SAMPLER_DESC linearSampler{0, D3D12_FILTER_MIN_MAG_MIP_LINEAR};
		linearSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		linearSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		linearSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		CD3DX12_ROOT_SIGNATURE_DESC desc(3, slotRootParameter, 1, &linearSampler,
										 D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		m_RootSignatures["voxelDebug"] = Utils::CreateRootSignature(m_Device, desc);
	}

	// clear voxel
	{
		CD3DX12_DESCRIPTOR_RANGE uavTable;
		uavTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);

		CD3DX12_ROOT_PARAMETER slotRootParameter[1];
		slotRootParameter[0].InitAsDescriptorTable(1, &uavTable);

		CD3DX12_ROOT_SIGNATURE_DESC desc(1, slotRootParameter,
										 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

		m_RootSignatures["clearVoxel"] = Utils::CreateRootSignature(m_Device, desc);
	}

	// voxel mipmap
	{
		CD3DX12_DESCRIPTOR_RANGE descriptorRanges[] =
			{
				{D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0}, // previous mip
				{D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1, 0}, // current mip
			};

		CD3DX12_ROOT_PARAMETER slotRootParameter[2];
		slotRootParameter[0].InitAsDescriptorTable(1, &descriptorRanges[0]);
		slotRootParameter[1].InitAsDescriptorTable(1, &descriptorRanges[1]);

		CD3DX12_ROOT_SIGNATURE_DESC desc(2, slotRootParameter,
										 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

		m_RootSignatures["voxelMipmap"] = Utils::CreateRootSignature(m_Device, desc);
	}
}

void PipelineStateManager::BuildPSOs()
{
	// graphics PSO
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc;
		ZeroMemory(&desc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
		desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		desc.SampleMask = UINT_MAX;
		desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		desc.NumRenderTargets = 1;
		desc.RTVFormats[0] = BACK_BUFFER_FORMAT;
		desc.SampleDesc = {1, 0};
		desc.DSVFormat = DEPTH_STENCIL_FORMAT;

		// forward rendering
		{
			D3D12_GRAPHICS_PIPELINE_STATE_DESC forwardRenderingDesc = desc;
			forwardRenderingDesc.InputLayout = {m_InputLayouts["default"].data(), (UINT)m_InputLayouts["default"].size()};
			forwardRenderingDesc.pRootSignature = m_RootSignatures["forwardRendering"].Get();
			forwardRenderingDesc.VS = CD3DX12_SHADER_BYTECODE(m_VSByteCodes["default"].Get());
			forwardRenderingDesc.PS = CD3DX12_SHADER_BYTECODE(m_PSByteCodes["forwardRendering"].Get());

			ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&forwardRenderingDesc, IID_PPV_ARGS(&m_PSOs["forwardRendering"])));

			D3D12_RENDER_TARGET_BLEND_DESC blendDesc = {};
			blendDesc.BlendEnable = true;
			blendDesc.LogicOpEnable = false;
			blendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
			blendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
			blendDesc.BlendOp = D3D12_BLEND_OP_ADD;
			blendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
			blendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
			blendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
			blendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
			blendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

			forwardRenderingDesc.BlendState.RenderTarget[0] = blendDesc;
			ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&forwardRenderingDesc, IID_PPV_ARGS(&m_PSOs["forwardRenderingTransparent"])));
		}

		// shadow pass
		{
			D3D12_GRAPHICS_PIPELINE_STATE_DESC shadowPassDesc = desc;
			shadowPassDesc.InputLayout = {m_InputLayouts["default"].data(), (UINT)m_InputLayouts["default"].size()};
			shadowPassDesc.pRootSignature = m_RootSignatures["shadow"].Get();
			shadowPassDesc.VS = CD3DX12_SHADER_BYTECODE(m_VSByteCodes["shadow"].Get());
			shadowPassDesc.PS = CD3DX12_SHADER_BYTECODE(m_PSByteCodes["shadow"].Get());

			shadowPassDesc.RasterizerState.DepthBias = 100000;
			shadowPassDesc.RasterizerState.DepthBiasClamp = 10.0f;
			shadowPassDesc.RasterizerState.SlopeScaledDepthBias = 1.0f;

			shadowPassDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
			shadowPassDesc.NumRenderTargets = 0;

			ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&shadowPassDesc, IID_PPV_ARGS(&m_PSOs["shadow"])));
		}

		// defered rendering g-buffer pass
		{
			D3D12_GRAPHICS_PIPELINE_STATE_DESC gbufferDesc = desc;
			gbufferDesc.InputLayout = {m_InputLayouts["default"].data(), (UINT)m_InputLayouts["default"].size()};
			gbufferDesc.pRootSignature = m_RootSignatures["gbuffer"].Get();
			gbufferDesc.VS = CD3DX12_SHADER_BYTECODE(m_VSByteCodes["default"].Get());
			gbufferDesc.PS = CD3DX12_SHADER_BYTECODE(m_PSByteCodes["gbuffer"].Get());
			gbufferDesc.NumRenderTargets = 5;
			gbufferDesc.RTVFormats[0] = GBUFFER_ALBEDO_FORMAT;
			gbufferDesc.RTVFormats[1] = GBUFFER_NORMAL_FORMAT;
			gbufferDesc.RTVFormats[2] = GBUFFER_METALNESS_FORMAT;
			gbufferDesc.RTVFormats[3] = GBUFFER_ROUGHNESS_FORMAT;
			gbufferDesc.RTVFormats[4] = GBUFFER_AMBIENT_FORMAT;

			ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&gbufferDesc, IID_PPV_ARGS(&m_PSOs["gbuffer"])));
		}

		// defered rendering ambient light pass
		{
			D3D12_GRAPHICS_PIPELINE_STATE_DESC deferredAmbientDesc = desc;
			deferredAmbientDesc.InputLayout = {m_InputLayouts["default"].data(), (UINT)m_InputLayouts["default"].size()};
			deferredAmbientDesc.pRootSignature = m_RootSignatures["deferredAmbientLight"].Get();
			deferredAmbientDesc.VS = CD3DX12_SHADER_BYTECODE(m_VSByteCodes["deferredAmbientLight"].Get());
			deferredAmbientDesc.PS = CD3DX12_SHADER_BYTECODE(m_PSByteCodes["deferredAmbientLight"].Get());
			deferredAmbientDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
			deferredAmbientDesc.DepthStencilState.DepthEnable = false;

			ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&deferredAmbientDesc, IID_PPV_ARGS(&m_PSOs["deferredAmbientLight"])));
		}

		// deferred rendering lighting pass 0
		{
			D3D12_GRAPHICS_PIPELINE_STATE_DESC deferredLightingPass0 = desc;
			deferredLightingPass0.InputLayout = {m_InputLayouts["default"].data(), (UINT)m_InputLayouts["default"].size()};
			deferredLightingPass0.pRootSignature = m_RootSignatures["deferredRenderingLightingPass0"].Get();
			deferredLightingPass0.VS = CD3DX12_SHADER_BYTECODE(m_VSByteCodes["lightingPass"].Get());
			// Only write to depth/stencil buffer, so no pixel shader is bound.

			// Only render the front faces of the light volumes.
			deferredLightingPass0.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;

			deferredLightingPass0.DepthStencilState.DepthEnable = true;
			deferredLightingPass0.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
			deferredLightingPass0.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;

			deferredLightingPass0.DepthStencilState.StencilEnable = true;
			deferredLightingPass0.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
			deferredLightingPass0.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;

			deferredLightingPass0.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			deferredLightingPass0.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_DECR_SAT;
			deferredLightingPass0.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
			deferredLightingPass0.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;

			// We are not rendering backfacing polygons, so BackFace settings do not matter.

			ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&deferredLightingPass0, IID_PPV_ARGS(&m_PSOs["deferredRenderingLightingPass0"])));
		}

		// deferred rendering lighting pass 1
		{
			D3D12_GRAPHICS_PIPELINE_STATE_DESC deferredLightingPass1 = desc;
			deferredLightingPass1.InputLayout = {m_InputLayouts["default"].data(), (UINT)m_InputLayouts["default"].size()};
			deferredLightingPass1.pRootSignature = m_RootSignatures["deferredRenderingLightingPass1"].Get();
			deferredLightingPass1.VS = CD3DX12_SHADER_BYTECODE(m_VSByteCodes["lightingPass"].Get());
			deferredLightingPass1.PS = CD3DX12_SHADER_BYTECODE(m_PSByteCodes["lightingPass"].Get());

			// Only render the back faces of the light volumes.
			deferredLightingPass1.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
			// Also render light volumes that are exceeding the far clipping plane.
			deferredLightingPass1.RasterizerState.DepthClipEnable = false;

			deferredLightingPass1.DepthStencilState.DepthEnable = true;
			deferredLightingPass1.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
			deferredLightingPass1.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;

			deferredLightingPass1.DepthStencilState.StencilEnable = true;
			deferredLightingPass1.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
			deferredLightingPass1.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;

			deferredLightingPass1.DepthStencilState.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;
			deferredLightingPass1.DepthStencilState.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
			deferredLightingPass1.DepthStencilState.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
			deferredLightingPass1.DepthStencilState.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;

			// We are not rendering frontfacing polygons, so FrontFace settings do not matter.

			D3D12_RENDER_TARGET_BLEND_DESC blendDesc = {};
			blendDesc.BlendEnable = true;
			blendDesc.LogicOpEnable = false;
			blendDesc.SrcBlend = D3D12_BLEND_ONE;
			blendDesc.DestBlend = D3D12_BLEND_ONE;
			blendDesc.BlendOp = D3D12_BLEND_OP_ADD;
			blendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
			blendDesc.DestBlendAlpha = D3D12_BLEND_ONE;
			blendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
			blendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
			blendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
			deferredLightingPass1.BlendState.RenderTarget[0] = blendDesc;

			ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&deferredLightingPass1, IID_PPV_ARGS(&m_PSOs["deferredRenderingLightingPass1"])));
		}

		// skybox PSO
		{
			D3D12_GRAPHICS_PIPELINE_STATE_DESC skyboxDesc = desc;
			skyboxDesc.InputLayout = {m_InputLayouts["skybox"].data(), (UINT)m_InputLayouts["skybox"].size()};
			skyboxDesc.pRootSignature = m_RootSignatures["skybox"].Get();
			skyboxDesc.VS = CD3DX12_SHADER_BYTECODE(m_VSByteCodes["skybox"].Get());
			skyboxDesc.PS = CD3DX12_SHADER_BYTECODE(m_PSByteCodes["skybox"].Get());

			// camera is inside the skybox, so just turn off the culling
			skyboxDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

			// skybox is drawn with depth value of z = 1 (NDC), which will fail the depth test
			// if the depth buffer was cleared to 1 and the depth function is LESS
			skyboxDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

			ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&skyboxDesc, IID_PPV_ARGS(&m_PSOs["skybox"])));
		}

		// velocity buffer
		{
			D3D12_GRAPHICS_PIPELINE_STATE_DESC velocityDesc = desc;
			velocityDesc.InputLayout = {m_InputLayouts["default"].data(), (UINT)m_InputLayouts["default"].size()};
			velocityDesc.pRootSignature = m_RootSignatures["velocityBuffer"].Get();
			velocityDesc.RTVFormats[0] = VELOCITY_BUFFER_FORMAT;
			velocityDesc.VS = CD3DX12_SHADER_BYTECODE(m_VSByteCodes["velocityBuffer"].Get());
			velocityDesc.PS = CD3DX12_SHADER_BYTECODE(m_PSByteCodes["velocityBuffer"].Get());

			ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&velocityDesc, IID_PPV_ARGS(&m_PSOs["velocityBuffer"])));
		}

		// draw normal
		{
			D3D12_GRAPHICS_PIPELINE_STATE_DESC drawNormalDesc = desc;
			drawNormalDesc.InputLayout = {m_InputLayouts["default"].data(), (UINT)m_InputLayouts["default"].size()};
			drawNormalDesc.pRootSignature = m_RootSignatures["drawNormal"].Get();
			drawNormalDesc.RTVFormats[0] = GBUFFER_NORMAL_FORMAT;
			drawNormalDesc.VS = CD3DX12_SHADER_BYTECODE(m_VSByteCodes["default"].Get());
			drawNormalDesc.PS = CD3DX12_SHADER_BYTECODE(m_PSByteCodes["drawNormal"].Get());

			ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&drawNormalDesc, IID_PPV_ARGS(&m_PSOs["drawNormal"])));
		}

		// debug
		{
			D3D12_GRAPHICS_PIPELINE_STATE_DESC debugPSO = desc;
			debugPSO.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
			debugPSO.DepthStencilState.DepthEnable = false;
			debugPSO.pRootSignature = m_RootSignatures["debug"].Get();
			debugPSO.RTVFormats[0] = BACK_BUFFER_FORMAT;
			debugPSO.VS = CD3DX12_SHADER_BYTECODE(m_VSByteCodes["debug"].Get());
			debugPSO.PS = CD3DX12_SHADER_BYTECODE(m_PSByteCodes["debug"].Get());

			ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&debugPSO, IID_PPV_ARGS(&m_PSOs["debug"])));
		}
	}

	// clear voxel
	{
		Blob byteCode = Utils::CompileShader(L"shaders\\clearVoxel.hlsl", nullptr, "main", "cs_5_0");

		D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = m_RootSignatures["clearVoxel"].Get();
		psoDesc.CS = CD3DX12_SHADER_BYTECODE(byteCode.Get());
		ThrowIfFailed(m_Device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&m_PSOs["clearVoxel"])));
	}

	// voxelizer
	{
		auto voxelVS = Utils::CompileShader(L"shaders\\voxelize.hlsl", nullptr, "VS", "vs_5_0");
		auto voxelGS = Utils::CompileShader(L"shaders\\voxelize.hlsl", nullptr, "GS", "gs_5_0");
		auto voxelPS = Utils::CompileShader(L"shaders\\voxelize.hlsl", nullptr, "PS", "ps_5_0");

		D3D12_GRAPHICS_PIPELINE_STATE_DESC voxelizeDesc = {};
		voxelizeDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		voxelizeDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON;
		voxelizeDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		voxelizeDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		voxelizeDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		voxelizeDesc.DepthStencilState.DepthEnable = false;
		voxelizeDesc.SampleMask = UINT_MAX;
		voxelizeDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		voxelizeDesc.NumRenderTargets = 0;
		voxelizeDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
		voxelizeDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
		voxelizeDesc.SampleDesc = {1, 0};
		voxelizeDesc.InputLayout = {m_InputLayouts["default"].data(), (UINT)m_InputLayouts["default"].size()};
		voxelizeDesc.pRootSignature = m_RootSignatures["voxelize"].Get();
		voxelizeDesc.VS = CD3DX12_SHADER_BYTECODE(voxelVS.Get());
		voxelizeDesc.GS = CD3DX12_SHADER_BYTECODE(voxelGS.Get());
		voxelizeDesc.PS = CD3DX12_SHADER_BYTECODE(voxelPS.Get());

		ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&voxelizeDesc, IID_PPV_ARGS(&m_PSOs["voxelize"])));
	}

	// debug voxel
	{
		auto voxelVS = Utils::CompileShader(L"shaders\\voxelDebug.hlsl", nullptr, "VS", "vs_5_0");
		auto voxelGS = Utils::CompileShader(L"shaders\\voxelDebug.hlsl", nullptr, "GS", "gs_5_0");
		auto voxelPS = Utils::CompileShader(L"shaders\\voxelDebug.hlsl", nullptr, "PS", "ps_5_0");

		D3D12_GRAPHICS_PIPELINE_STATE_DESC voxelizeDesc = {};
		voxelizeDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		// voxelizeDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
		voxelizeDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		voxelizeDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		voxelizeDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		voxelizeDesc.SampleMask = UINT_MAX;
		voxelizeDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
		voxelizeDesc.NumRenderTargets = 1;
		voxelizeDesc.RTVFormats[0] = BACK_BUFFER_FORMAT;
		voxelizeDesc.DSVFormat = DEPTH_STENCIL_FORMAT;
		voxelizeDesc.SampleDesc = {1, 0};
		voxelizeDesc.pRootSignature = m_RootSignatures["voxelDebug"].Get();
		voxelizeDesc.VS = CD3DX12_SHADER_BYTECODE(voxelVS.Get());
		voxelizeDesc.GS = CD3DX12_SHADER_BYTECODE(voxelGS.Get());
		voxelizeDesc.PS = CD3DX12_SHADER_BYTECODE(voxelPS.Get());

		ThrowIfFailed(m_Device->CreateGraphicsPipelineState(&voxelizeDesc, IID_PPV_ARGS(&m_PSOs["voxelDebug"])));
	}

	// voxel mipmap
	{
		Blob byteCode = Utils::CompileShader(L"shaders\\voxelMipmap.hlsl", nullptr, "main", "cs_5_0");

		D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = m_RootSignatures["voxelMipmap"].Get();
		psoDesc.CS = CD3DX12_SHADER_BYTECODE(byteCode.Get());
		ThrowIfFailed(m_Device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&m_PSOs["voxelMipmap"])));
	}

	// voxel compute average
	{
		Blob byteCode = Utils::CompileShader(L"shaders\\voxelComputeAverage.hlsl", nullptr, "main", "cs_5_0");

		D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = m_RootSignatures["voxelMipmap"].Get();
		psoDesc.CS = CD3DX12_SHADER_BYTECODE(byteCode.Get());
		ThrowIfFailed(m_Device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&m_PSOs["voxelComputeAverage"])));
	}
}

std::vector<CD3DX12_STATIC_SAMPLER_DESC> PipelineStateManager::GetStaticSamplers()
{
	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		0,								  // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,  // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	return {linearWrap};
}