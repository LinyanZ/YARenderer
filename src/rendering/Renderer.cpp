#include "pch.h"
#include "Renderer.h"
#include "RenderingSettings.h"

extern RenderingSettings g_RenderingSettings;

Renderer::Renderer(Ref<DxContext> dxContext, UINT width, UINT height)
	: m_DxContext(dxContext), m_Width(width), m_Height(height)
{
	UINT passCount = 1;
	UINT objectCount = 3;
	UINT materialCount = 200;
	UINT lightCount = 1;

	for (int i = 0; i < NUM_FRAMES_IN_FLIGHT; ++i)
		m_FrameResources.push_back(std::make_unique<FrameResource>(dxContext->GetDevice(), passCount, objectCount, materialCount, lightCount));

	PipelineStates::Init(dxContext->GetDevice());

	m_EnvironmentMap = std::make_unique<EnvironmentMap>(dxContext);
	m_CascadedShadowMap = std::make_unique<CascadedShadowMap>(dxContext);
	m_PostProcessing = std::make_unique<PostProcessing>(dxContext, width, height);
	m_SSAO = std::make_unique<SSAO>(dxContext, width, height);
	m_TAA = std::make_unique<TAA>(dxContext, width, height);
	m_VXGI = std::make_unique<VXGI>(dxContext, VOXEL_DIMENSION);

	BuildResources();
	AllocateDescriptors();
	BuildDescriptors();

	m_ScreenViewport = {0, 0, (float)width, float(height), 0.0f, 1.0f};
	m_ScissorRect = {0, 0, (long)width, (long)height};
	m_Camera.SetLens(0.25f * MathHelper::Pi, (float)width / height, 1.0f, 1000.0f);
}

void Renderer::BuildResources()
{
	m_GBufferAlbedo = Texture::Create(m_DxContext->GetDevice(), m_Width, m_Height, 1, GBUFFER_ALBEDO_FORMAT, 1);
	m_GBufferNormal = Texture::Create(m_DxContext->GetDevice(), m_Width, m_Height, 1, GBUFFER_NORMAL_FORMAT, 1);
	m_GBufferMetalness = Texture::Create(m_DxContext->GetDevice(), m_Width, m_Height, 1, GBUFFER_METALNESS_FORMAT, 1);
	m_GBufferRoughness = Texture::Create(m_DxContext->GetDevice(), m_Width, m_Height, 1, GBUFFER_ROUGHNESS_FORMAT, 1);
	m_GBufferAmbient = Texture::Create(m_DxContext->GetDevice(), m_Width, m_Height, 1, GBUFFER_AMBIENT_FORMAT, 1);
	m_GBufferVelocity = Texture::Create(m_DxContext->GetDevice(), m_Width, m_Height, 1, GBUFFER_VELOCITY_FORMAT, 1);
}

void Renderer::AllocateDescriptors()
{
	m_GBufferAlbedo.Rtv = m_DxContext->GetRtvHeap().Alloc();
	m_GBufferNormal.Rtv = m_DxContext->GetRtvHeap().Alloc();
	m_GBufferMetalness.Rtv = m_DxContext->GetRtvHeap().Alloc();
	m_GBufferRoughness.Rtv = m_DxContext->GetRtvHeap().Alloc();
	m_GBufferAmbient.Rtv = m_DxContext->GetRtvHeap().Alloc();
	m_GBufferVelocity.Rtv = m_DxContext->GetRtvHeap().Alloc();

	m_GBufferAlbedo.Srv = m_DxContext->GetCbvSrvUavHeap().Alloc();
	m_GBufferNormal.Srv = m_DxContext->GetCbvSrvUavHeap().Alloc();
	m_GBufferMetalness.Srv = m_DxContext->GetCbvSrvUavHeap().Alloc();
	m_GBufferRoughness.Srv = m_DxContext->GetCbvSrvUavHeap().Alloc();
	m_GBufferAmbient.Srv = m_DxContext->GetCbvSrvUavHeap().Alloc();
	m_GBufferVelocity.Srv = m_DxContext->GetCbvSrvUavHeap().Alloc();
}

void Renderer::BuildDescriptors()
{
	m_GBufferAlbedo.CreateSrv(m_DxContext->GetDevice(), D3D12_SRV_DIMENSION_TEXTURE2D, 0, 1);
	m_GBufferAlbedo.CreateRtv(m_DxContext->GetDevice(), D3D12_RTV_DIMENSION_TEXTURE2D);

	m_GBufferNormal.CreateSrv(m_DxContext->GetDevice(), D3D12_SRV_DIMENSION_TEXTURE2D, 0, 1);
	m_GBufferNormal.CreateRtv(m_DxContext->GetDevice(), D3D12_RTV_DIMENSION_TEXTURE2D);

	m_GBufferMetalness.CreateSrv(m_DxContext->GetDevice(), D3D12_SRV_DIMENSION_TEXTURE2D, 0, 1);
	m_GBufferMetalness.CreateRtv(m_DxContext->GetDevice(), D3D12_RTV_DIMENSION_TEXTURE2D);

	m_GBufferRoughness.CreateSrv(m_DxContext->GetDevice(), D3D12_SRV_DIMENSION_TEXTURE2D, 0, 1);
	m_GBufferRoughness.CreateRtv(m_DxContext->GetDevice(), D3D12_RTV_DIMENSION_TEXTURE2D);

	m_GBufferAmbient.CreateSrv(m_DxContext->GetDevice(), D3D12_SRV_DIMENSION_TEXTURE2D, 0, 1);
	m_GBufferAmbient.CreateRtv(m_DxContext->GetDevice(), D3D12_RTV_DIMENSION_TEXTURE2D);

	m_GBufferVelocity.CreateSrv(m_DxContext->GetDevice(), D3D12_SRV_DIMENSION_TEXTURE2D, 0, 1);
	m_GBufferVelocity.CreateRtv(m_DxContext->GetDevice(), D3D12_RTV_DIMENSION_TEXTURE2D);
}

void Renderer::Setup()
{
	BuildRenderItems();
	BuildLightingDataBuffer();

	m_EnvironmentMap->Load("resources/textures/kloppenheim_06_puresky_4k.hdr");
	m_Camera.SetPosition(-2.29, 5.11, 1.15);
}

void Renderer::OnUpdate(Timer &timer)
{
	OnKeyboardInput(timer.DeltaTime());
	m_Camera.UpdateViewMatrix();

	// LOG_INFO("Camera: {} {} {}", XMVectorGetX(m_Camera.GetPosition()), XMVectorGetY(m_Camera.GetPosition()), XMVectorGetZ(m_Camera.GetPosition()));

	UpdateLights(timer);
	UpdateObjectConstantBuffers();
	UpdateMainPassConstantBuffer(timer);
	UpdateMaterialConstantBuffer();
	UpdateSSAOConstantBuffer();

	m_CascadedShadowMap->CalcOrthoProjs(m_Camera, m_Lights[0]);
	UpdateShadowPassCB();
}

void Renderer::BeginFrame()
{
	m_CurrFrameResourceIndex = (m_CurrFrameResourceIndex + 1) % NUM_FRAMES_IN_FLIGHT;
	m_DxContext->WaitForFenceValue(CurrFrameResource()->Fence);

	auto commandList = m_DxContext->GetCommandList();
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_DxContext->CurrentBackBuffer().Resource.Get(),
																		  D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
}

void Renderer::Render()
{
	auto commandList = m_DxContext->GetCommandList();

	// set the descriptor heap and a universal root signature thanks to Bindless Rendering
	ID3D12DescriptorHeap *descriptorHeaps[] = {m_DxContext->GetCbvSrvUavHeap().Get()};
	commandList->SetDescriptorHeaps(1, descriptorHeaps);

	commandList->SetGraphicsRootSignature(PipelineStates::GetRootSignature());

	// bind pass constant buffer
	auto passCB = CurrFrameResource()->PassCB->GetResource();
	commandList->SetGraphicsRootConstantBufferView((UINT)RootParam::PassCB, passCB->GetGPUVirtualAddress());

	// bind lighting data
	auto lightCB = CurrFrameResource()->LightBuffer->GetResource();
	commandList->SetGraphicsRootConstantBufferView((UINT)RootParam::LightCB, lightCB->GetGPUVirtualAddress());

	// bind shadow constant buffer
	auto shadowCB = CurrFrameResource()->ShadowCB->GetResource();
	commandList->SetGraphicsRootConstantBufferView((UINT)RootParam::ShadowCB, shadowCB->GetGPUVirtualAddress());

	// cascaded shadow from directional light
	ShadowMapPass(commandList);

	// reset taa so that it won't use outdated information
	if (g_RenderingSettings.GI.DebugVoxel || g_RenderingSettings.AntialisingMethod != Antialising::TAA)
		m_TAA->Reset();

	// re-voxelize the whole scene if required
	if (g_RenderingSettings.GI.DynamicUpdate)
	{
		commandList->SetComputeRootSignature(PipelineStates::GetRootSignature());
		VoxelizeScene(commandList);
	}

	if (g_RenderingSettings.GI.DebugVoxel)
	{
		DebugVoxel(commandList);
	}
	else
	{
		GBufferPass(commandList);

		commandList->ClearRenderTargetView(m_DxContext->CurrentBackBuffer().Rtv.CPUHandle, Colors::Black, 0, nullptr);
		commandList->OMSetRenderTargets(1, &m_DxContext->CurrentBackBuffer().Rtv.CPUHandle, true, &m_DxContext->DepthStencilBuffer().Dsv.CPUHandle);

		DeferredLightingPass(commandList);

		DrawSkybox(commandList);

		if (g_RenderingSettings.AntialisingMethod == Antialising::TAA)
			m_TAA->Render(commandList, m_GBufferVelocity);

		m_PostProcessing->Render(commandList, m_DxContext->CurrentBackBuffer(), m_GBufferVelocity);
	}

	// Debug(commandList, m_EnvironmentMap->GetBRDFLUT().Srv, 0);
}

void Renderer::EndFrame()
{
	auto commandList = m_DxContext->GetCommandList();
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_DxContext->CurrentBackBuffer().Resource.Get(),
																		  D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	CurrFrameResource()->Fence = m_DxContext->ExecuteCommandList();
	m_DxContext->Present(g_RenderingSettings.EnableVSync);
}

//
// Setup
//

XMFLOAT4 CalcSunDir(float thetaDeg, float phiDeg)
{
	float thetaRad = thetaDeg * MathHelper::Pi / 180.0;
	float phiRad = phiDeg * MathHelper::Pi / 180.0;

	float x = sin(phiRad) * cos(thetaRad);
	float z = sin(phiRad) * sin(thetaRad);
	float y = cos(phiRad);

	return XMFLOAT4(-x, -y, -z, 0);
}

void Renderer::BuildLightingDataBuffer()
{
	m_Lights.resize(1);

	m_Lights[0].DirectionWS = CalcSunDir(g_RenderingSettings.SunTheta, g_RenderingSettings.SunPhi);
	m_Lights[0].Color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	m_Lights[0].Enabled = TRUE;
	m_Lights[0].Intensity = g_RenderingSettings.SunLightIntensity;
	m_Lights[0].Type = Light::Type::DirectionalLight;

	/*m_Lights[1].PositionWS = XMFLOAT4(6.0f, 0.0f, 0.0f, 1.0f);
	m_Lights[1].Color = XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f);
	m_Lights[1].Enabled = TRUE;
	m_Lights[1].Intensity = 4;
	m_Lights[1].Type = Light::Type::PointLight;
	m_Lights[1].Range = 5;

	m_Lights[2].PositionWS = XMFLOAT4(-6.0f, 0.0f, 0.0f, 1.0f);
	m_Lights[2].Color = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);
	m_Lights[2].Enabled = TRUE;
	m_Lights[2].Intensity = 4;
	m_Lights[2].Type = Light::Type::PointLight;
	m_Lights[2].Range = 5;*/
}

void Renderer::BuildRenderItems()
{
	auto commandList = m_DxContext->GetCommandList();
	auto device = m_DxContext->GetDevice();
	auto &cbvSrvUavHeap = m_DxContext->GetCbvSrvUavHeap();

	m_Skybox = Mesh::FromFile("resources/meshes/skybox.gltf");
	m_Skybox->UploadVertexAndIndexBufferToGPU(device, commandList);

#if TEST_SCENE
	Ref<RenderItem> testScene = std::make_shared<RenderItem>();
	testScene->Mesh = Mesh::FromFile("resources/low_poly_winter_scene/scene.gltf");
	testScene->Mesh->UploadVertexAndIndexBufferToGPU(device, commandList);
	testScene->Mesh->LoadTextures(device, commandList, cbvSrvUavHeap);
	testScene->objCBIndex = 0;
	testScene->matCBIndex = 0;
	m_RenderItems.push_back(testScene);
#endif

#if SPONZA_SCENE
	Ref<RenderItem> sponza = std::make_shared<RenderItem>();
	sponza->Mesh = Mesh::FromFile("resources/sponza/NewSponza_Main_glTF_002.gltf");
	sponza->Mesh->UploadVertexAndIndexBufferToGPU(device, commandList);
	sponza->Mesh->LoadTextures(device, commandList, cbvSrvUavHeap);
	sponza->objCBIndex = 0;
	sponza->matCBIndex = 0;

	m_RenderItems.push_back(sponza);

	Ref<RenderItem> sponzaCurtain = std::make_shared<RenderItem>();
	sponzaCurtain->Mesh = Mesh::FromFile("resources/sponza/NewSponza_Curtains_glTF.gltf");
	sponzaCurtain->Mesh->UploadVertexAndIndexBufferToGPU(device, commandList);
	sponzaCurtain->Mesh->LoadTextures(device, commandList, cbvSrvUavHeap);
	sponzaCurtain->objCBIndex = 1;
	sponzaCurtain->matCBIndex = sponza->Mesh->Materials().size();

	m_RenderItems.push_back(sponzaCurtain);
#endif

	m_DxContext->ExecuteCommandList();
	m_DxContext->Flush();
}

//
// Rendering
//

void Renderer::GBufferPass(GraphicsCommandList commandList)
{
	commandList->RSSetViewports(1, &m_ScreenViewport);
	commandList->RSSetScissorRects(1, &m_ScissorRect);

	commandList->SetPipelineState(PipelineStates::GetPSO("gbuffer"));

	D3D12_RESOURCE_BARRIER preBarriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(m_GBufferAlbedo.Resource.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET),
			CD3DX12_RESOURCE_BARRIER::Transition(m_GBufferNormal.Resource.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET),
			CD3DX12_RESOURCE_BARRIER::Transition(m_GBufferMetalness.Resource.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET),
			CD3DX12_RESOURCE_BARRIER::Transition(m_GBufferRoughness.Resource.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET),
			CD3DX12_RESOURCE_BARRIER::Transition(m_GBufferAmbient.Resource.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET),
			CD3DX12_RESOURCE_BARRIER::Transition(m_GBufferVelocity.Resource.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET),
		};
	commandList->ResourceBarrier(6, preBarriers);

	commandList->ClearRenderTargetView(m_GBufferAlbedo.Rtv.CPUHandle, XMVECTORF32{0.0f, 0.0f, 0.0f, 0.0f}, 0, nullptr);
	commandList->ClearRenderTargetView(m_GBufferNormal.Rtv.CPUHandle, XMVECTORF32{0.0f, 0.0f, 0.0f, 0.0f}, 0, nullptr);
	commandList->ClearRenderTargetView(m_GBufferMetalness.Rtv.CPUHandle, XMVECTORF32{0.0f, 0.0f, 0.0f, 0.0f}, 0, nullptr);
	commandList->ClearRenderTargetView(m_GBufferRoughness.Rtv.CPUHandle, XMVECTORF32{0.0f, 0.0f, 0.0f, 0.0f}, 0, nullptr);
	commandList->ClearRenderTargetView(m_GBufferAmbient.Rtv.CPUHandle, XMVECTORF32{0.0f, 0.0f, 0.0f, 0.0f}, 0, nullptr);
	commandList->ClearRenderTargetView(m_GBufferVelocity.Rtv.CPUHandle, XMVECTORF32{0.0f, 0.0f, 0.0f, 0.0f}, 0, nullptr);
	commandList->ClearDepthStencilView(m_DxContext->DepthStencilBuffer().Dsv.CPUHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	commandList->OMSetRenderTargets(6, &m_GBufferAlbedo.Rtv.CPUHandle, true, &m_DxContext->DepthStencilBuffer().Dsv.CPUHandle);

	// Bind IBL textures
	// commandList->SetGraphicsRootDescriptorTable(7, m_EnvironmentMap->GetIrMap().Srv.GPUHandle);

	DrawRenderItems(commandList);

	D3D12_RESOURCE_BARRIER postBarriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(m_GBufferAlbedo.Resource.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON),
			CD3DX12_RESOURCE_BARRIER::Transition(m_GBufferNormal.Resource.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON),
			CD3DX12_RESOURCE_BARRIER::Transition(m_GBufferMetalness.Resource.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON),
			CD3DX12_RESOURCE_BARRIER::Transition(m_GBufferRoughness.Resource.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON),
			CD3DX12_RESOURCE_BARRIER::Transition(m_GBufferAmbient.Resource.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON),
			CD3DX12_RESOURCE_BARRIER::Transition(m_GBufferVelocity.Resource.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON),
		};
	commandList->ResourceBarrier(6, postBarriers);
}

void Renderer::DeferredLightingPass(GraphicsCommandList commandList)
{
	commandList->SetPipelineState(PipelineStates::GetPSO("deferredLighting"));

	UINT resources[] = {m_GBufferAlbedo.Srv.Index,
						m_GBufferNormal.Srv.Index,
						m_GBufferMetalness.Srv.Index,
						m_GBufferRoughness.Srv.Index,
						m_GBufferAmbient.Srv.Index,
						m_DxContext->DepthStencilBuffer().Srv.Index,
						m_CascadedShadowMap->Srv(4).Index,
						m_VXGI->GetTextureSrv(g_RenderingSettings.GI.SecondBounce ? 1 : 0).Index,
						m_EnvironmentMap->GetIrMap().Srv.Index,
						m_EnvironmentMap->GetSpMap().Srv.Index,
						m_EnvironmentMap->GetBRDFLUT().Srv.Index};

	commandList->SetGraphicsRoot32BitConstants((UINT)RootParam::RenderResources, sizeof(resources) / sizeof(UINT), resources, 0);

	// fullscreen triangle
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->DrawInstanced(3, 1, 0, 0);
}

void Renderer::DrawRenderItems(GraphicsCommandList commandList, bool transparent)
{
	auto objectCB = CurrFrameResource()->ObjectCB->GetResource();
	UINT objCBByteSize = Utils::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	auto matCB = CurrFrameResource()->MatCB->GetResource();
	UINT matCBByteSize = Utils::CalcConstantBufferByteSize(sizeof(MaterialConstants));

	for (auto &ritem : m_RenderItems)
	{
		// set object constant buffer
		auto objCBAddress = objectCB->GetGPUVirtualAddress() + ritem->objCBIndex * objCBByteSize;
		commandList->SetGraphicsRootConstantBufferView((UINT)RootParam::ObjectCB, objCBAddress);

		auto &mesh = ritem->Mesh;
		commandList->IASetVertexBuffers(0, 1, &mesh->VertexBufferView());
		commandList->IASetIndexBuffer(&mesh->IndexBufferView());
		commandList->IASetPrimitiveTopology(ritem->PrimitiveType);

		for (const auto &submesh : mesh->SubMeshes())
		{
			if (submesh.Transparent != transparent)
				continue;

			// set material constant buffer
			auto matCBAddress = matCB->GetGPUVirtualAddress() + (submesh.MaterialIndex + ritem->matCBIndex) * matCBByteSize;
			auto &material = mesh->Materials()[submesh.MaterialIndex];

			commandList->SetGraphicsRootConstantBufferView((UINT)RootParam::MatCB, matCBAddress);
			commandList->DrawIndexedInstanced(submesh.IndexCount, 1,
											  submesh.StartIndexLocation, submesh.BaseVertexLocation, 0);
		}
	}
}

void Renderer::ShadowMapPass(GraphicsCommandList commandList)
{
	commandList->RSSetViewports(1, &m_CascadedShadowMap->Viewport());
	commandList->RSSetScissorRects(1, &m_CascadedShadowMap->ScissorRect());

	commandList->SetPipelineState(PipelineStates::GetPSO("shadow"));

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_CascadedShadowMap->GetResource(),
																		  D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE));

	for (UINT i = 0; i < NUM_CASCADES; i++)
	{
		// Bind cascade shadow index
		commandList->SetGraphicsRoot32BitConstants((UINT)RootParam::RenderResources, 1, &i, 0);

		commandList->ClearDepthStencilView(m_CascadedShadowMap->Dsv(i).CPUHandle,
										   D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
		commandList->OMSetRenderTargets(0, nullptr, false, &m_CascadedShadowMap->Dsv(i).CPUHandle);

		DrawRenderItems(commandList);
	}

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_CascadedShadowMap->GetResource(),
																		  D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ));
}

void Renderer::DrawSkybox(GraphicsCommandList commandList)
{
	commandList->SetPipelineState(PipelineStates::GetPSO("skybox"));
	commandList->SetGraphicsRoot32BitConstants((UINT)RootParam::RenderResources, 1,
											   &m_EnvironmentMap->GetEnvMap().Srv.Index, 0);

	commandList->IASetVertexBuffers(0, 1, &m_Skybox->VertexBufferView());
	commandList->IASetIndexBuffer(&m_Skybox->IndexBufferView());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	commandList->DrawIndexedInstanced(m_Skybox->SubMeshes()[0].IndexCount, 1, 0, 0, 0);
}

void Renderer::VoxelizeScene(GraphicsCommandList commandList)
{
	commandList->OMSetRenderTargets(0, nullptr, false, nullptr);

	commandList->RSSetViewports(1, &m_VXGI->GetViewPort());
	commandList->RSSetScissorRects(1, &m_VXGI->GetScissorRect());

	commandList->SetPipelineState(PipelineStates::GetPSO("voxelize"));

	UINT resources[] = {m_VXGI->GetVoxelBufferUav().Index, m_CascadedShadowMap->Srv(4).Index};
	commandList->SetGraphicsRoot32BitConstants((UINT)RootParam::RenderResources, sizeof(resources) / sizeof(UINT), resources, 0);

	DrawRenderItems(commandList, false);

	m_VXGI->BufferToTexture3D(commandList);
}

void Renderer::DebugVoxel(GraphicsCommandList commandList)
{
	commandList->ClearRenderTargetView(m_DxContext->CurrentBackBuffer().Rtv.CPUHandle, XMVECTORF32{0.0f, 0.0f, 0.0f, 1.0f}, 0, nullptr);
	commandList->ClearDepthStencilView(m_DxContext->DepthStencilBuffer().Dsv.CPUHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	commandList->RSSetViewports(1, &m_ScreenViewport);
	commandList->RSSetScissorRects(1, &m_ScissorRect);

	commandList->OMSetRenderTargets(1, &m_DxContext->CurrentBackBuffer().Rtv.CPUHandle, true, &m_DxContext->DepthStencilBuffer().Dsv.CPUHandle);
	commandList->SetPipelineState(PipelineStates::GetPSO("voxelDebug"));

	UINT resources[] = {m_VXGI->GetTextureSrv(g_RenderingSettings.GI.SecondBounce ? 1 : 0).Index, static_cast<UINT>(g_RenderingSettings.GI.DebugVoxelMipLevel)};
	commandList->SetGraphicsRoot32BitConstants((UINT)RootParam::RenderResources, sizeof(resources) / sizeof(UINT), resources, 0);

	UINT dimension = VOXEL_DIMENSION / pow(2, g_RenderingSettings.GI.DebugVoxelMipLevel);

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
	commandList->DrawInstanced(pow(dimension, 3), 1, 0, 0);
}

void Renderer::Debug(GraphicsCommandList commandList, Descriptor srv, UINT slot)
{
	commandList->RSSetViewports(1, &m_ScreenViewport);
	commandList->RSSetScissorRects(1, &m_ScissorRect);

	commandList->OMSetRenderTargets(1, &m_DxContext->CurrentBackBuffer().Rtv.CPUHandle, true, nullptr);

	commandList->SetPipelineState(PipelineStates::GetPSO("debug"));

	UINT resources[] = {srv.Index, slot};
	commandList->SetGraphicsRoot32BitConstants((UINT)RootParam::RenderResources, sizeof(resources) / sizeof(UINT), resources, 0);

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->DrawInstanced(6, 1, 0, 0);
}

//
// Input
//

void Renderer::OnKeyboardInput(float dt)
{
	float moveSpeed = 10.0f;

	if (GetAsyncKeyState(VK_LSHIFT) & 0x8000)
		moveSpeed *= 5;

	if (GetAsyncKeyState('W') & 0x8000)
		m_Camera.Walk(moveSpeed * dt);

	if (GetAsyncKeyState('S') & 0x8000)
		m_Camera.Walk(-moveSpeed * dt);

	if (GetAsyncKeyState('A') & 0x8000)
		m_Camera.Strafe(-moveSpeed * dt);

	if (GetAsyncKeyState('D') & 0x8000)
		m_Camera.Strafe(moveSpeed * dt);
}

void Renderer::OnMouseInput(int dxPixel, int dyPixel)
{
	// Make each pixel correspond to a quarter of a degree.
	float dx = XMConvertToRadians(0.1f * static_cast<float>(dxPixel));
	float dy = XMConvertToRadians(0.1f * static_cast<float>(dyPixel));

	m_Camera.Pitch(dy);
	m_Camera.RotateY(dx);
}

//
// Update Logics
//

void Renderer::UpdateLights(Timer &timer)
{
	m_Lights[0].DirectionWS = CalcSunDir(g_RenderingSettings.SunTheta, g_RenderingSettings.SunPhi);
	m_Lights[0].Intensity = g_RenderingSettings.SunLightIntensity;

	auto lightBuffer = CurrFrameResource()->LightBuffer.get();

	XMMATRIX view = m_Camera.GetView();

	for (int i = 0; i < m_Lights.size(); i++)
	{
		auto &light = m_Lights[i];
		light.PositionWS.x += XMScalarCos(timer.TotalTime() * 2) * timer.DeltaTime() * 3;

		auto PositionWS = XMLoadFloat4(&light.PositionWS);
		auto DirectionWS = XMLoadFloat4(&light.DirectionWS);

		XMStoreFloat4(&light.PositionVS, XMVector4Transform(PositionWS, view));
		XMStoreFloat4(&light.DirectionVS, XMVector4Transform(DirectionWS, view));

		lightBuffer->CopyData(i, light);
	}
}

void Renderer::UpdateObjectConstantBuffers()
{
	auto objectCB = CurrFrameResource()->ObjectCB.get();

	for (auto &ritem : m_RenderItems)
	{
		//// Only update the cbuffer data if the constants have changed.
		//// This needs to be tracked per frame resource.
		// if (ritem->NumFramesDirty > 0)
		//{
		//	XMMATRIX world = XMLoadFloat4x4(&ritem->World);

		//	ObjectConstants objConstants;
		//	XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
		//	XMStoreFloat4x4(&objConstants.PrevWorld, XMMatrixTranspose(world));
		//	objectCB->CopyData(ritem->objCBIndex, objConstants);

		//	// Next FrameResource need to be updated too.
		//	ritem->NumFramesDirty--;
		//}

		XMMATRIX world = XMLoadFloat4x4(&ritem->World);

		ObjectConstants objConstants;
		XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
		XMStoreFloat4x4(&objConstants.PrevWorld, XMMatrixTranspose(world));
		objectCB->CopyData(ritem->objCBIndex, objConstants);
	}
}

void Renderer::UpdateMainPassConstantBuffer(Timer &timer)
{
	XMMATRIX view = m_Camera.GetView();
	XMMATRIX proj = m_Camera.GetProj();

	// apply jitter if using taa
	float jitterX = 0, jitterY = 0;

	if (!g_RenderingSettings.GI.DebugVoxel && g_RenderingSettings.AntialisingMethod == Antialising::TAA)
	{
		float haltonX = 2.0f * RenderingUtils::Halton(m_JitterIndex + 1, 2) - 1.0f;
		float haltonY = 2.0f * RenderingUtils::Halton(m_JitterIndex + 1, 3) - 1.0f;
		jitterX = (haltonX / m_Width);
		jitterY = (haltonY / m_Height);

		XMMATRIX jitterTranslation = XMMatrixTranslation(jitterX, jitterY, 0);
		proj = XMMatrixMultiply(proj, jitterTranslation);
	}

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
	XMMATRIX T(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);

	XMMATRIX projTex = XMMatrixMultiply(proj, T);

	XMStoreFloat4x4(&m_MainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&m_MainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&m_MainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&m_MainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&m_MainPassCB.ViewProjMatrix, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&m_MainPassCB.PrevViewProjMatrix, XMMatrixTranspose(m_PrevViewProjMatrix));
	XMStoreFloat4x4(&m_MainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	XMStoreFloat4x4(&m_MainPassCB.ProjTex, XMMatrixTranspose(projTex));
	m_MainPassCB.EyePosW = m_Camera.GetPosition3f();
	m_MainPassCB.RenderTargetSize = XMFLOAT2((float)m_Width, (float)m_Height);
	m_MainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / m_Width, 1.0f / m_Height);
	m_MainPassCB.NearZ = 1.0f;
	m_MainPassCB.FarZ = 1000.0f;
	m_MainPassCB.TotalTime = timer.TotalTime();
	m_MainPassCB.DeltaTime = timer.DeltaTime();
	m_MainPassCB.EnableGI = g_RenderingSettings.GI.Enable;
	m_MainPassCB.EnableIBL = g_RenderingSettings.EnableIBL;

	m_MainPassCB.Jitter = XMFLOAT2(jitterX, jitterY);
	m_MainPassCB.PreviousJitter = XMFLOAT2(m_PreviousJitterX, m_PreviousJitterY);

	m_PrevViewProjMatrix = viewProj;
	m_PreviousJitterX = jitterX;
	m_PreviousJitterY = jitterY;
	m_JitterIndex++;
	m_JitterIndex %= 16;

	auto currPassCB = CurrFrameResource()->PassCB.get();
	currPassCB->CopyData(0, m_MainPassCB);
}

void Renderer::UpdateMaterialConstantBuffer()
{
	auto matCB = CurrFrameResource()->MatCB.get();

	for (auto ritem : m_RenderItems)
	{
		auto mesh = ritem->Mesh;
		for (int i = 0; i < mesh->Materials().size(); i++)
			matCB->CopyData(ritem->matCBIndex + i, mesh->Materials()[i].BuildMaterialConstants());
	}
}

void Renderer::UpdateSSAOConstantBuffer()
{
	SSAOConstants ssaoCB;

	XMMATRIX view = m_Camera.GetView();
	XMMATRIX proj = m_Camera.GetProj();

	// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
	XMMATRIX T(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);

	ssaoCB.Proj = m_MainPassCB.Proj;
	ssaoCB.InvProj = m_MainPassCB.InvProj;
	XMStoreFloat4x4(&ssaoCB.ProjTex, XMMatrixTranspose(proj * T));

	auto blurWeights = m_SSAO->CalcGaussWeights(2.5f);
	ssaoCB.BlurWeights[0] = XMFLOAT4(&blurWeights[0]);
	ssaoCB.BlurWeights[1] = XMFLOAT4(&blurWeights[4]);
	ssaoCB.BlurWeights[2] = XMFLOAT4(&blurWeights[8]);

	ssaoCB.InvRenderTargetSize = XMFLOAT2(1.0f / m_SSAO->SSAOMapWidth(), 1.0f / m_SSAO->SSAOMapHeight());

	// Coordinates given in view space.
	ssaoCB.OcclusionRadius = 0.05f;
	ssaoCB.OcclusionFadeStart = 0.2f;
	ssaoCB.OcclusionFadeEnd = 1.0f;
	ssaoCB.SurfaceEpsilon = 0.05f;

	auto currSSAOCB = CurrFrameResource()->SSAOCB.get();
	currSSAOCB->CopyData(0, ssaoCB);
}

void Renderer::UpdateShadowPassCB()
{
	for (int i = 0; i < NUM_CASCADES; i++)
	{
		XMMATRIX viewProj = m_CascadedShadowMap->ViewProjMatrix(i);
		XMStoreFloat4x4(&m_ShadowPassCB.ViewProjMatrix[i], XMMatrixTranspose(viewProj));
		m_ShadowPassCB.CascadeRadius[i] = m_CascadedShadowMap->CascadeRadius(i);
		m_ShadowPassCB.CascadeEnds[i] = m_CascadedShadowMap->CascadeEnds(i);
	}

	m_ShadowPassCB.CascadeEnds[NUM_CASCADES] = m_CascadedShadowMap->CascadeEnds(NUM_CASCADES);
	m_ShadowPassCB.TransitionRatio = g_RenderingSettings.CascadeTransitionRatio;
	m_ShadowPassCB.Softness = g_RenderingSettings.ShadowSoftness;
	m_ShadowPassCB.ShowCascades = g_RenderingSettings.ShowCascades;
	m_ShadowPassCB.UseVogelDiskSample = g_RenderingSettings.UseVogelDiskSample;
	m_ShadowPassCB.NumSamples = g_RenderingSettings.NumSamples;

	auto currPassCB = CurrFrameResource()->ShadowCB.get();
	currPassCB->CopyData(0, m_ShadowPassCB);
}

//
// Resize Logics
//

void Renderer::OnResize(UINT width, UINT height)
{
	if (width == m_Width && height == m_Height)
		return;

	m_Width = width;
	m_Height = height;

	ResizeResources();
	BuildDescriptors();

	m_ScreenViewport = {0, 0, (float)width, float(height), 0.0f, 1.0f};
	m_ScissorRect = {0, 0, (long)width, (long)height};
	m_Camera.SetLens(0.25f * MathHelper::Pi, (float)width / height, 1.0f, 1000.0f);

	m_SSAO->OnResize(width, height);
	m_PostProcessing->OnResize(width, height);
	m_TAA->OnResize(width, height);
}

void Renderer::ResizeResources()
{
	m_GBufferAlbedo.Resize(m_DxContext->GetDevice(), m_Width, m_Height);
	m_GBufferNormal.Resize(m_DxContext->GetDevice(), m_Width, m_Height);
	m_GBufferMetalness.Resize(m_DxContext->GetDevice(), m_Width, m_Height);
	m_GBufferRoughness.Resize(m_DxContext->GetDevice(), m_Width, m_Height);
	m_GBufferAmbient.Resize(m_DxContext->GetDevice(), m_Width, m_Height);
	m_GBufferVelocity.Resize(m_DxContext->GetDevice(), m_Width, m_Height);
}