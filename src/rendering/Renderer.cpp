#include "pch.h"
#include "Renderer.h"
#include "RenderingSettings.h"

constexpr auto PI = 3.1415926;

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

	m_PipelineStateManager = std::make_unique<PipelineStateManager>(dxContext->GetDevice());
	m_EnvironmentMap = std::make_unique<EnvironmentMap>(dxContext);
	m_CascadeShadowMap = std::make_unique<CascadeShadowMap>(dxContext);
	m_PostProcessing = std::make_unique<PostProcessing>(dxContext, width, height);
	m_SSAO = std::make_unique<SSAO>(dxContext, width, height);
	m_TAA = std::make_unique<TAA>(dxContext, width, height);

	BuildResources();
	AllocateDescriptors();
	BuildDescriptors();

	m_ScreenViewport = {0, 0, (float)width, float(height), 0.0f, 1.0f};
	m_ScissorRect = {0, 0, (long)width, (long)height};
	m_Camera.SetLens(0.25f * MathHelper::Pi, (float)width / height, 1.0f, 1000.0f);

	RenderingUtils::Init(dxContext);

	m_VolumeAlbedo.Init(dxContext, VOXEL_DIMENSION);
}

void Renderer::BuildResources()
{
	m_GBufferAlbedo = Texture::Create(m_DxContext->GetDevice(), m_Width, m_Height, 1, GBUFFER_ALBEDO_FORMAT, 1);
	m_GBufferNormal = Texture::Create(m_DxContext->GetDevice(), m_Width, m_Height, 1, GBUFFER_NORMAL_FORMAT, 1);
	m_GBufferAmbient = Texture::Create(m_DxContext->GetDevice(), m_Width, m_Height, 1, GBUFFER_AMBIENT_FORMAT, 1);
	m_GBufferRoughness = Texture::Create(m_DxContext->GetDevice(), m_Width, m_Height, 1, GBUFFER_ROUGHNESS_FORMAT, 1);
	m_GBufferMetalness = Texture::Create(m_DxContext->GetDevice(), m_Width, m_Height, 1, GBUFFER_METALNESS_FORMAT, 1);

	m_VelocityBuffer = Texture::Create(m_DxContext->GetDevice(), m_Width, m_Height, 1, VELOCITY_BUFFER_FORMAT, 1);
}

void Renderer::AllocateDescriptors()
{
	m_GBufferAlbedo.Rtv = m_DxContext->GetRtvHeap().Alloc();
	m_GBufferNormal.Rtv = m_DxContext->GetRtvHeap().Alloc();
	m_GBufferMetalness.Rtv = m_DxContext->GetRtvHeap().Alloc();
	m_GBufferRoughness.Rtv = m_DxContext->GetRtvHeap().Alloc();
	m_GBufferAmbient.Rtv = m_DxContext->GetRtvHeap().Alloc();

	m_GBufferAlbedo.Srv = m_DxContext->GetCbvSrvUavHeap().Alloc();
	m_GBufferNormal.Srv = m_DxContext->GetCbvSrvUavHeap().Alloc();
	m_GBufferMetalness.Srv = m_DxContext->GetCbvSrvUavHeap().Alloc();
	m_GBufferRoughness.Srv = m_DxContext->GetCbvSrvUavHeap().Alloc();
	m_GBufferAmbient.Srv = m_DxContext->GetCbvSrvUavHeap().Alloc();

	m_VelocityBuffer.Rtv = m_DxContext->GetRtvHeap().Alloc();
	m_VelocityBuffer.Srv = m_DxContext->GetCbvSrvUavHeap().Alloc();
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

	m_VelocityBuffer.CreateSrv(m_DxContext->GetDevice(), D3D12_SRV_DIMENSION_TEXTURE2D, 0, 1);
	m_VelocityBuffer.CreateRtv(m_DxContext->GetDevice(), D3D12_RTV_DIMENSION_TEXTURE2D);
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

	m_CascadeShadowMap->CalcOrthoProjs(m_Camera, m_Lights[0]);
	UpdateShadowPassCB();
}

void Renderer::BeginFrame()
{
	m_DxContext->WaitForFenceValue(CurrFrameResource()->Fence);

	auto commandList = m_DxContext->GetCommandList();
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_DxContext->CurrentBackBuffer().Resource.Get(),
																		  D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
}

void Renderer::Render()
{
	auto commandList = m_DxContext->GetCommandList();

	ID3D12DescriptorHeap *descriptorHeaps[] = {m_DxContext->GetCbvSrvUavHeap().Get()};
	commandList->SetDescriptorHeaps(1, descriptorHeaps);
	commandList->SetGraphicsRootSignature(m_PipelineStateManager->GetRootSignature("universal").Get());

	commandList->RSSetViewports(1, &m_ScreenViewport);
	commandList->RSSetScissorRects(1, &m_ScissorRect);

	commandList->ClearDepthStencilView(m_DxContext->DepthStencilBuffer().Dsv.CPUHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	commandList->OMSetRenderTargets(1, &m_DxContext->CurrentBackBuffer().Rtv.CPUHandle, true, &m_DxContext->DepthStencilBuffer().Dsv.CPUHandle);

	DrawSkybox(commandList);
	// VelocityBufferPass(commandList);
	// ShadowMapPass(commandList);

	// if (g_RenderingSettings.DynamicUpdate)
	// {
	// 	ClearVoxel(commandList);
	// 	VoxelizeScene(commandList);
	// 	VoxelComputeAverage(commandList);
	// 	GenVoxelMipmap(commandList);
	// }

	// if (g_RenderingSettings.DebugVoxel)
	// {
	// 	DebugVoxel(commandList);

	// 	// Reset for TAA
	// 	m_FirstFrame = true;
	// }
	// else
	// {
	// 	ForwardRendering(commandList);
	// 	DeferredRendering(commandList);

	// 	DrawSkybox(commandList);
	// 	m_TAA->Render(commandList, m_VelocityBuffer, m_FirstFrame);

	// 	m_PostProcessing->Render(commandList, m_DxContext->CurrentBackBuffer(), m_VelocityBuffer);
	// }

	// Debug(commandList, m_SSAO->AmbientMap().Srv, 0);
	// Debug(commandList, m_CascadeShadowMap->Srv(0), 12);
	// Debug(commandList, m_CascadeShadowMap->Srv(1), 13);
	// Debug(commandList, m_CascadeShadowMap->Srv(2), 14);
	// Debug(commandList, m_CascadeShadowMap->Srv(3), 15);
}

void Renderer::EndFrame()
{
	auto commandList = m_DxContext->GetCommandList();
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_DxContext->CurrentBackBuffer().Resource.Get(),
																		  D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	UINT64 nextFence = m_DxContext->ExecuteCommandList();
	CurrFrameResource()->Fence = nextFence;

	m_DxContext->Present(g_RenderingSettings.EnableVSync);

	m_CurrFrameResourceIndex = (m_CurrFrameResourceIndex + 1) % NUM_FRAMES_IN_FLIGHT;
	if (!g_RenderingSettings.DebugVoxel)
		m_FirstFrame = false;
}

//
// Setup
//

XMFLOAT4 CalcSunDir(float thetaDeg, float phiDeg)
{
	float thetaRad = thetaDeg * PI / 180.0;
	float phiRad = phiDeg * PI / 180.0;

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
	// testScene->Mesh = Mesh::FromFile("resources/meshes/test_scene2.gltf");
	testScene->Mesh = Mesh::FromFile("resources/city/scene.gltf");
	testScene->Mesh->UploadVertexAndIndexBufferToGPU(device, commandList);
	testScene->Mesh->LoadTextures(device, commandList, cbvSrvUavHeap);
	testScene->objCBIndex = 0;
	testScene->matCBIndex = 0;
	XMStoreFloat4x4(&testScene->World, XMMatrixScaling(0.1f, 0.1f, 0.1f));
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

void Renderer::ForwardRendering(GraphicsCommandList commandList)
{
	commandList->RSSetViewports(1, &m_ScreenViewport);
	commandList->RSSetScissorRects(1, &m_ScissorRect);

	DrawNormalsAndDepth(commandList);
	m_SSAO->ComputeSSAO(commandList.Get(), m_GBufferNormal, m_DxContext->DepthStencilBuffer().Srv, CurrFrameResource(), 3);

	// SSAO is rendered at half of the render target size, so reset to fullscreen size.
	commandList->RSSetViewports(1, &m_ScreenViewport);
	commandList->RSSetScissorRects(1, &m_ScissorRect);

	// Clear the background
	// commandList->ClearRenderTargetView(m_DxContext->CurrentBackBuffer().Rtv.CPUHandle, XMVECTORF32{ 0.0f, 0.0f, 0.0f, 1.0f }, 0, nullptr);
	commandList->ClearDepthStencilView(m_DxContext->DepthStencilBuffer().Dsv.CPUHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Specify the buffers we are going to render to.
	commandList->OMSetRenderTargets(1, &m_DxContext->CurrentBackBuffer().Rtv.CPUHandle, true, &m_DxContext->DepthStencilBuffer().Dsv.CPUHandle);

	// Set the root signature for forward rendering
	commandList->SetGraphicsRootSignature(m_PipelineStateManager->GetRootSignature("forwardRendering").Get());

	// Bind pass constant buffer
	auto passCB = CurrFrameResource()->PassCB->GetResource();
	commandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());

	// Bind IBL textures
	commandList->SetGraphicsRootDescriptorTable(7, m_EnvironmentMap->GetIrMap().Srv.GPUHandle);

	// Bind SSAO map
	commandList->SetGraphicsRootDescriptorTable(8, m_SSAO->AmbientMap().Srv.GPUHandle);

	// Bind Shadow map
	commandList->SetGraphicsRootDescriptorTable(9, m_CascadeShadowMap->Srv(4).GPUHandle);

	// Bind lighting data
	auto lightingDataBuffer = CurrFrameResource()->LightBuffer->GetResource();
	commandList->SetGraphicsRootShaderResourceView(10, lightingDataBuffer->GetGPUVirtualAddress());

	// Bind cascade shadow constant buffer
	auto shadowCB = CurrFrameResource()->ShadowCB->GetResource();
	commandList->SetGraphicsRootConstantBufferView(11, shadowCB->GetGPUVirtualAddress());

	commandList->SetGraphicsRootDescriptorTable(12, m_VolumeAlbedo.Srv.GPUHandle);

	// Draw Opague Objects
	commandList->SetPipelineState(m_PipelineStateManager->GetPSO("forwardRendering").Get());
	DrawRenderItems(commandList, RenderPass::ForwardRendering, false);

	// Draw transparent objects
	commandList->SetPipelineState(m_PipelineStateManager->GetPSO("forwardRenderingTransparent").Get());
	DrawRenderItems(commandList, RenderPass::ForwardRendering, true);
}

void Renderer::DeferredRendering(GraphicsCommandList commandList)
{
	ShadowMapPass(commandList);

	// CascadeShadowMap is rendered at a different render target size, so reset to fullscreen size.
	commandList->RSSetViewports(1, &m_ScreenViewport);
	commandList->RSSetScissorRects(1, &m_ScissorRect);

	// Store g-buffer resources (albedo, normal, metalness, roughness, ambient light)
	GBufferPass(commandList);

	m_SSAO->ComputeSSAO(commandList.Get(), m_GBufferNormal, m_DxContext->DepthStencilBuffer().Srv, CurrFrameResource(), 3);

	// SSAO is rendered at half of the render target size, so reset to fullscreen size.
	commandList->RSSetViewports(1, &m_ScreenViewport);
	commandList->RSSetScissorRects(1, &m_ScissorRect);

	// Draw ambient light to the back buffer, with SSAO applied.
	AmbientLightPass(commandList);

	// Additively blend direct lighting to the back buffer.
	LightingPass(commandList);
}

void Renderer::GBufferPass(GraphicsCommandList commandList)
{
	// Indicate a state transition on the resource usage.
	D3D12_RESOURCE_BARRIER preBarriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(m_GBufferAlbedo.Resource.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET),
			CD3DX12_RESOURCE_BARRIER::Transition(m_GBufferNormal.Resource.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET),
			CD3DX12_RESOURCE_BARRIER::Transition(m_GBufferMetalness.Resource.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET),
			CD3DX12_RESOURCE_BARRIER::Transition(m_GBufferRoughness.Resource.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET),
			CD3DX12_RESOURCE_BARRIER::Transition(m_GBufferAmbient.Resource.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET),
		};
	commandList->ResourceBarrier(5, preBarriers);

	// Clear the background.
	commandList->ClearRenderTargetView(m_GBufferAlbedo.Rtv.CPUHandle, XMVECTORF32{0.0f, 0.0f, 0.0f, 0.0f}, 0, nullptr);
	commandList->ClearRenderTargetView(m_GBufferNormal.Rtv.CPUHandle, XMVECTORF32{0.0f, 0.0f, 0.0f, 0.0f}, 0, nullptr);
	commandList->ClearRenderTargetView(m_GBufferMetalness.Rtv.CPUHandle, XMVECTORF32{0.0f, 0.0f, 0.0f, 0.0f}, 0, nullptr);
	commandList->ClearRenderTargetView(m_GBufferRoughness.Rtv.CPUHandle, XMVECTORF32{0.0f, 0.0f, 0.0f, 0.0f}, 0, nullptr);
	commandList->ClearRenderTargetView(m_GBufferAmbient.Rtv.CPUHandle, XMVECTORF32{0.0f, 0.0f, 0.0f, 0.0f}, 0, nullptr);
	commandList->ClearDepthStencilView(m_DxContext->DepthStencilBuffer().Dsv.CPUHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Specify the buffers we are going to render to.
	commandList->OMSetRenderTargets(5, &m_GBufferAlbedo.Rtv.CPUHandle, true, &m_DxContext->DepthStencilBuffer().Dsv.CPUHandle);

	commandList->SetGraphicsRootSignature(m_PipelineStateManager->GetRootSignature("gbuffer").Get());
	commandList->SetPipelineState(m_PipelineStateManager->GetPSO("gbuffer").Get());

	// Bind pass constant buffer
	auto passCB = CurrFrameResource()->PassCB->GetResource();
	commandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());

	// Bind IBL textures
	commandList->SetGraphicsRootDescriptorTable(7, m_EnvironmentMap->GetIrMap().Srv.GPUHandle);

	DrawRenderItems(commandList);

	D3D12_RESOURCE_BARRIER postBarriers[] =
		{
			CD3DX12_RESOURCE_BARRIER::Transition(m_GBufferAlbedo.Resource.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON),
			CD3DX12_RESOURCE_BARRIER::Transition(m_GBufferNormal.Resource.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON),
			CD3DX12_RESOURCE_BARRIER::Transition(m_GBufferMetalness.Resource.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON),
			CD3DX12_RESOURCE_BARRIER::Transition(m_GBufferRoughness.Resource.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON),
			CD3DX12_RESOURCE_BARRIER::Transition(m_GBufferAmbient.Resource.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON),
		};
	commandList->ResourceBarrier(5, postBarriers);
}

void Renderer::AmbientLightPass(GraphicsCommandList commandList)
{
	commandList->ClearRenderTargetView(m_DxContext->CurrentBackBuffer().Rtv.CPUHandle, Colors::Black, 0, nullptr);
	commandList->OMSetRenderTargets(1, &m_DxContext->CurrentBackBuffer().Rtv.CPUHandle, true, nullptr);

	commandList->SetGraphicsRootSignature(m_PipelineStateManager->GetRootSignature("deferredAmbientLight").Get());
	commandList->SetPipelineState(m_PipelineStateManager->GetPSO("deferredAmbientLight").Get());

	commandList->SetGraphicsRootDescriptorTable(0, m_GBufferAmbient.Srv.GPUHandle);
	commandList->SetGraphicsRootDescriptorTable(1, m_SSAO->AmbientMap().Srv.GPUHandle);

	commandList->DrawInstanced(3, 1, 0, 0);
}

void Renderer::LightingPass(GraphicsCommandList commandList)
{
	for (UINT i = 0; i < m_Lights.size(); i++)
	{
		if (!m_Lights[i].Enabled)
			continue;

		// Clear the stencilbuffer to 1 to mark all pixels.
		commandList->ClearDepthStencilView(m_DxContext->DepthStencilBuffer().Dsv.CPUHandle, D3D12_CLEAR_FLAG_STENCIL, 1.0f, 1, 0, nullptr);

		// Only bind the depth/stencil buffer.
		commandList->OMSetRenderTargets(0, nullptr, false, &m_DxContext->DepthStencilBuffer().Dsv.CPUHandle);

		// Lighting pass 0: unmark pixels that are in front of the front faces of the light volume.
		commandList->SetPipelineState(m_PipelineStateManager->GetPSO("deferredRenderingLightingPass0").Get());
		commandList->SetGraphicsRootSignature(m_PipelineStateManager->GetRootSignature("deferredRenderingLightingPass0").Get());

		// Set light index
		commandList->SetGraphicsRoot32BitConstant(0, i, 0);

		// Set pass constant buffer
		auto passCB = CurrFrameResource()->PassCB->GetResource();
		commandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());

		// Bind lighting data
		auto lightingDataBuffer = CurrFrameResource()->LightBuffer->GetResource();
		commandList->SetGraphicsRootShaderResourceView(2, lightingDataBuffer->GetGPUVirtualAddress());

		// Draw light geometry
		switch (m_Lights[i].Type)
		{
		case Light::Type::PointLight:
			commandList->IASetVertexBuffers(0, 1, &m_PointLightGeo->VertexBufferView());
			commandList->IASetIndexBuffer(&m_PointLightGeo->IndexBufferView());

			commandList->DrawIndexedInstanced((UINT)m_PointLightGeo->Indices().size(), 1, 0, 0, 0);
			break;
		case Light::Type::DirectionalLight:
		case Light::Type::SpotLight:
		default:
			break;
		}

		// Lighting pass 1: shade pixels that are marked and in front of the back faces of the light volume.
		commandList->OMSetStencilRef(1);
		commandList->OMSetRenderTargets(1, &m_DxContext->CurrentBackBuffer().Rtv.CPUHandle, true, &m_DxContext->DepthStencilBuffer().Dsv.CPUHandle);

		commandList->SetGraphicsRootSignature(m_PipelineStateManager->GetRootSignature("deferredRenderingLightingPass1").Get());
		commandList->SetPipelineState(m_PipelineStateManager->GetPSO("deferredRenderingLightingPass1").Get());

		commandList->SetGraphicsRootDescriptorTable(3, m_GBufferAlbedo.Srv.GPUHandle);
		commandList->SetGraphicsRootDescriptorTable(4, m_DxContext->DepthStencilBuffer().Srv.GPUHandle);

		// Draw light geometry
		switch (m_Lights[i].Type)
		{
		case Light::Type::PointLight:
			commandList->IASetVertexBuffers(0, 1, &m_PointLightGeo->VertexBufferView());
			commandList->IASetIndexBuffer(&m_PointLightGeo->IndexBufferView());

			commandList->DrawIndexedInstanced((UINT)m_PointLightGeo->Indices().size(), 1, 0, 0, 0);
			break;
		case Light::Type::DirectionalLight:
		case Light::Type::SpotLight:
		default:
			break;
		}
	}
}

void Renderer::DrawNormalsAndDepth(GraphicsCommandList commandList)
{
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_GBufferNormal.Resource.Get(),
																		  D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET));

	float clearValue[] = {0.0f, 0.0f, 1.0f, 0.0f};
	commandList->ClearRenderTargetView(m_GBufferNormal.Rtv.CPUHandle, clearValue, 0, nullptr);
	commandList->ClearDepthStencilView(m_DxContext->DepthStencilBuffer().Dsv.CPUHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	commandList->OMSetRenderTargets(1, &m_GBufferNormal.Rtv.CPUHandle, true, &m_DxContext->DepthStencilBuffer().Dsv.CPUHandle);

	commandList->SetGraphicsRootSignature(m_PipelineStateManager->GetRootSignature("drawNormal").Get());
	commandList->SetPipelineState(m_PipelineStateManager->GetPSO("drawNormal").Get());

	auto passCB = CurrFrameResource()->PassCB->GetResource();
	commandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());

	DrawRenderItems(commandList, RenderPass::NormalOnly);

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_GBufferNormal.Resource.Get(),
																		  D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON));
}

void Renderer::DrawRenderItems(GraphicsCommandList commandList, RenderPass pass, bool transparent)
{
	auto objectCB = CurrFrameResource()->ObjectCB->GetResource();
	UINT objCBByteSize = Utils::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	auto matCB = CurrFrameResource()->MatCB->GetResource();
	UINT matCBByteSize = Utils::CalcConstantBufferByteSize(sizeof(MaterialConstants));

	for (auto &ritem : m_RenderItems)
	{
		// Set object constant buffer
		auto objCBAddress = objectCB->GetGPUVirtualAddress() + ritem->objCBIndex * objCBByteSize;
		commandList->SetGraphicsRootConstantBufferView(0, objCBAddress);

		auto &mesh = ritem->Mesh;
		commandList->IASetVertexBuffers(0, 1, &mesh->VertexBufferView());
		commandList->IASetIndexBuffer(&mesh->IndexBufferView());
		commandList->IASetPrimitiveTopology(ritem->PrimitiveType);

		for (const auto &submesh : mesh->SubMeshes())
		{
			// Set material constant buffer
			auto matCBAddress = matCB->GetGPUVirtualAddress() + (submesh.MaterialIndex + ritem->matCBIndex) * matCBByteSize;
			auto &material = mesh->Materials()[submesh.MaterialIndex];

			switch (pass)
			{
			case RenderPass::NormalOnly:
				commandList->SetGraphicsRootConstantBufferView(2, matCBAddress);
				if (material.HasNormalTexture)
					commandList->SetGraphicsRootDescriptorTable(3, material.NormalTexture.Srv.GPUHandle);
				break;
			case RenderPass::ForwardRendering:
				// Skip transparent objects for opague rendering (and vice versa)
				if (submesh.Transparent != transparent)
					continue;

				commandList->SetGraphicsRootConstantBufferView(2, matCBAddress);
				if (material.HasAlbedoTexture)
					commandList->SetGraphicsRootDescriptorTable(3, material.AlbedoTexture.Srv.GPUHandle);
				if (material.HasNormalTexture)
					commandList->SetGraphicsRootDescriptorTable(4, material.NormalTexture.Srv.GPUHandle);
				if (material.HasMetalnessTexture)
					commandList->SetGraphicsRootDescriptorTable(5, material.MetalnessTexture.Srv.GPUHandle);
				if (material.HasRoughnessTexture)
					commandList->SetGraphicsRootDescriptorTable(6, material.RoughnessTexture.Srv.GPUHandle);
				break;
			case RenderPass::Shadow:
			case RenderPass::Velocity:
			default:
				break;
			}

			commandList->DrawIndexedInstanced(submesh.IndexCount, 1,
											  submesh.StartIndexLocation, submesh.BaseVertexLocation, 0);
			;
		}
	}
}

void Renderer::ShadowMapPass(GraphicsCommandList commandList)
{
	commandList->RSSetViewports(1, &m_CascadeShadowMap->Viewport());
	commandList->RSSetScissorRects(1, &m_CascadeShadowMap->ScissorRect());

	commandList->SetGraphicsRootSignature(m_PipelineStateManager->GetRootSignature("shadow").Get());
	commandList->SetPipelineState(m_PipelineStateManager->GetPSO("shadow").Get());

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_CascadeShadowMap->GetResource(),
																		  D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE));

	// Set shadow pass constant
	auto passCB = CurrFrameResource()->ShadowCB->GetResource();
	commandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());

	for (int i = 0; i < NUM_CASCADES; i++)
	{
		// Bind cascade shadow index
		commandList->SetGraphicsRoot32BitConstant(2, i, 0);

		commandList->ClearDepthStencilView(m_CascadeShadowMap->Dsv(i).CPUHandle,
										   D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

		commandList->OMSetRenderTargets(0, nullptr, false, &m_CascadeShadowMap->Dsv(i).CPUHandle);

		DrawRenderItems(commandList, RenderPass::Shadow);
	}

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_CascadeShadowMap->GetResource(),
																		  D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ));
}

void Renderer::DrawSkybox(GraphicsCommandList commandList)
{
	struct SkyBoxRenderResource
	{
		UINT TexIndex;
	};

	SkyBoxRenderResource skyBoxRenderResource{m_EnvironmentMap->GetEnvMap().Srv.Index};

	commandList->SetPipelineState(m_PipelineStateManager->GetPSO("skybox").Get());

	// Set pass constant buffer
	auto passCB = CurrFrameResource()->PassCB->GetResource();
	commandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());

	commandList->SetGraphicsRoot32BitConstants(2, 1, reinterpret_cast<void *>(&skyBoxRenderResource), 0);

	commandList->IASetVertexBuffers(0, 1, &m_Skybox->VertexBufferView());
	commandList->IASetIndexBuffer(&m_Skybox->IndexBufferView());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	commandList->DrawIndexedInstanced(m_Skybox->SubMeshes()[0].IndexCount, 1, 0, 0, 0);
}

void Renderer::VelocityBufferPass(GraphicsCommandList commandList)
{
	commandList->RSSetViewports(1, &m_ScreenViewport);
	commandList->RSSetScissorRects(1, &m_ScissorRect);

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_VelocityBuffer.Resource.Get(),
																		  D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// Clear the background
	commandList->ClearRenderTargetView(m_VelocityBuffer.Rtv.CPUHandle, XMVECTORF32{0.0f, 0.0f, 0.0f, 1.0f}, 0, nullptr);
	commandList->ClearDepthStencilView(m_DxContext->DepthStencilBuffer().Dsv.CPUHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	commandList->OMSetRenderTargets(1, &m_VelocityBuffer.Rtv.CPUHandle, true, &m_DxContext->DepthStencilBuffer().Dsv.CPUHandle);

	commandList->SetGraphicsRootSignature(m_PipelineStateManager->GetRootSignature("velocityBuffer").Get());
	commandList->SetPipelineState(m_PipelineStateManager->GetPSO("velocityBuffer").Get());

	// Bind pass constant buffer
	auto passCB = CurrFrameResource()->PassCB->GetResource();
	commandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());

	DrawRenderItems(commandList, RenderPass::Velocity);

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_VelocityBuffer.Resource.Get(),
																		  D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON));
}

void Renderer::ClearVoxel(GraphicsCommandList commandList)
{
	commandList->SetComputeRootSignature(m_PipelineStateManager->GetRootSignature("clearVoxel").Get());
	commandList->SetPipelineState(m_PipelineStateManager->GetPSO("clearVoxel").Get());
	commandList->SetComputeRootDescriptorTable(0, m_VolumeAlbedo.Uav[0].GPUHandle);

	UINT groupSize = VOXEL_DIMENSION / 8;
	commandList->Dispatch(groupSize, groupSize, groupSize);
}

void Renderer::VoxelizeScene(GraphicsCommandList commandList)
{
	commandList->OMSetRenderTargets(0, nullptr, false, nullptr);
	commandList->RSSetScissorRects(1, &m_VolumeAlbedo.ScissorRect);
	commandList->RSSetViewports(1, &m_VolumeAlbedo.ViewPort);

	commandList->SetPipelineState(m_PipelineStateManager->GetPSO("voxelize").Get());
	commandList->SetGraphicsRootSignature(m_PipelineStateManager->GetRootSignature("voxelize").Get());

	// Bind pass constant buffer
	auto passCB = CurrFrameResource()->PassCB->GetResource();
	commandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());

	// Bind Shadow map
	commandList->SetGraphicsRootDescriptorTable(7, m_CascadeShadowMap->Srv(4).GPUHandle);

	// Bind lighting data
	auto lightingDataBuffer = CurrFrameResource()->LightBuffer->GetResource();
	commandList->SetGraphicsRootShaderResourceView(8, lightingDataBuffer->GetGPUVirtualAddress());

	// Bind cascade shadow constant buffer
	auto shadowCB = CurrFrameResource()->ShadowCB->GetResource();
	commandList->SetGraphicsRootConstantBufferView(9, shadowCB->GetGPUVirtualAddress());

	// Bind volumn texture albedo
	commandList->SetGraphicsRootDescriptorTable(10, m_VolumeAlbedo.Uav[0].GPUHandle);

	DrawRenderItems(commandList, RenderPass::ForwardRendering, false);
}

void Renderer::DebugVoxel(GraphicsCommandList commandList)
{
	commandList->ClearRenderTargetView(m_DxContext->CurrentBackBuffer().Rtv.CPUHandle, XMVECTORF32{0.0f, 0.0f, 0.0f, 1.0f}, 0, nullptr);
	commandList->ClearDepthStencilView(m_DxContext->DepthStencilBuffer().Dsv.CPUHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	commandList->RSSetViewports(1, &m_ScreenViewport);
	commandList->RSSetScissorRects(1, &m_ScissorRect);

	commandList->OMSetRenderTargets(1, &m_DxContext->CurrentBackBuffer().Rtv.CPUHandle, true, &m_DxContext->DepthStencilBuffer().Dsv.CPUHandle);
	commandList->SetGraphicsRootSignature(m_PipelineStateManager->GetRootSignature("voxelDebug").Get());
	commandList->SetPipelineState(m_PipelineStateManager->GetPSO("voxelDebug").Get());

	auto passCB = CurrFrameResource()->PassCB->GetResource();
	commandList->SetGraphicsRootConstantBufferView(0, passCB->GetGPUVirtualAddress());
	commandList->SetGraphicsRootDescriptorTable(1, m_VolumeAlbedo.Srv.GPUHandle);

	// Set mip level
	commandList->SetGraphicsRoot32BitConstants(2, 1, &g_RenderingSettings.DebugVoxelMipLevel, 0);

	UINT dimension = VOXEL_DIMENSION / pow(2, g_RenderingSettings.DebugVoxelMipLevel);

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
	commandList->DrawInstanced(pow(dimension, 3), 1, 0, 0);
}

void Renderer::VoxelComputeAverage(GraphicsCommandList commandList)
{
	commandList->SetPipelineState(m_PipelineStateManager->GetPSO("voxelComputeAverage").Get());
	commandList->SetComputeRootSignature(m_PipelineStateManager->GetRootSignature("voxelMipmap").Get()); // share the same root signature
	commandList->SetComputeRootDescriptorTable(0, m_VolumeAlbedo.Uav[0].GPUHandle);

	UINT count = std::max(VOXEL_DIMENSION / 8, 1);
	commandList->Dispatch(count, count, count);
}

void Renderer::GenVoxelMipmap(GraphicsCommandList commandList)
{
	UINT mipLevels = m_VolumeAlbedo.GetMipLevels();

	commandList->SetPipelineState(m_PipelineStateManager->GetPSO("voxelMipmap").Get());
	commandList->SetComputeRootSignature(m_PipelineStateManager->GetRootSignature("voxelMipmap").Get());

	for (int i = 1, levelWidth = VOXEL_DIMENSION / 2; i < mipLevels; i++, levelWidth /= 2)
	{
		commandList->SetComputeRootDescriptorTable(0, m_VolumeAlbedo.Uav[i - 1].GPUHandle);
		commandList->SetComputeRootDescriptorTable(1, m_VolumeAlbedo.Uav[i].GPUHandle);

		UINT count = std::max(levelWidth / 8, 1);
		commandList->Dispatch(count, count, count);
	}
}

void Renderer::Debug(GraphicsCommandList commandList, Descriptor srv, int slot)
{
	commandList->RSSetViewports(1, &m_ScreenViewport);
	commandList->RSSetScissorRects(1, &m_ScissorRect);
	commandList->OMSetRenderTargets(1, &m_DxContext->CurrentBackBuffer().Rtv.CPUHandle, true, nullptr);

	commandList->SetGraphicsRootSignature(m_PipelineStateManager->GetRootSignature("debug").Get());
	commandList->SetPipelineState(m_PipelineStateManager->GetPSO("debug").Get());
	commandList->SetGraphicsRootDescriptorTable(0, srv.GPUHandle);
	commandList->SetGraphicsRoot32BitConstant(1, (UINT)slot, 0);

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

		auto scale = XMMatrixScaling(0.1, 0.1, 0.1);

#if SPONZA_SCENE
		scale = XMMatrixScaling(1, 1, 1);
#endif

		world = XMMatrixMultiply(world, scale);

		ObjectConstants objConstants;
		XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
		XMStoreFloat4x4(&objConstants.PrevWorld, XMMatrixTranspose(world));
		objectCB->CopyData(ritem->objCBIndex, objConstants);
	}
}

void Renderer::UpdateMainPassConstantBuffer(Timer &timer)
{
	float haltonX = 2.0f * RenderingUtils::Halton(m_JitterIndex + 1, 2) - 1.0f;
	float haltonY = 2.0f * RenderingUtils::Halton(m_JitterIndex + 1, 3) - 1.0f;
	float jitterX = (haltonX / m_Width);
	float jitterY = (haltonY / m_Height);
	// jitterX = 0.0f;
	// jitterY = 0.0f;

	XMMATRIX view = m_Camera.GetView();
	XMMATRIX proj = m_Camera.GetProj();

	XMMATRIX jitterTranslation = XMMatrixTranslation(jitterX, jitterY, 0);
	proj = XMMatrixMultiply(proj, jitterTranslation);

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
	XMStoreFloat4x4(&m_MainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	XMStoreFloat4x4(&m_MainPassCB.ProjTex, XMMatrixTranspose(projTex));
	m_MainPassCB.EyePosW = m_Camera.GetPosition3f();
	m_MainPassCB.RenderTargetSize = XMFLOAT2((float)m_Width, (float)m_Height);
	m_MainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / m_Width, 1.0f / m_Height);
	m_MainPassCB.NearZ = 1.0f;
	m_MainPassCB.FarZ = 1000.0f;
	m_MainPassCB.TotalTime = timer.TotalTime();
	m_MainPassCB.DeltaTime = timer.DeltaTime();
	m_MainPassCB.Jitter = XMFLOAT2(jitterX, jitterY);
	m_MainPassCB.EnableGI = g_RenderingSettings.EnableGI;

	if (!m_FirstFrame)
	{
		XMStoreFloat4x4(&m_MainPassCB.PrevViewProjMatrix, XMMatrixTranspose(m_PrevViewProjMatrix));
		m_MainPassCB.PreviousJitter = XMFLOAT2(m_PreviousJitterX, m_PreviousJitterY);
	}

	auto currPassCB = CurrFrameResource()->PassCB.get();
	currPassCB->CopyData(0, m_MainPassCB);

	m_PrevViewProjMatrix = viewProj;
	m_PreviousJitterX = jitterX;
	m_PreviousJitterY = jitterY;
	m_JitterIndex++;
	m_JitterIndex %= 16;
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
		XMMATRIX viewProj = m_CascadeShadowMap->ViewProjMatrix(i);
		XMStoreFloat4x4(&m_ShadowPassCB.ViewProjMatrix[i], XMMatrixTranspose(viewProj));
		m_ShadowPassCB.CascadeRadius[i] = m_CascadeShadowMap->CascadeRadius(i);
		m_ShadowPassCB.CascadeEnds[i] = m_CascadeShadowMap->CascadeEnds(i);
	}

	m_ShadowPassCB.CascadeEnds[NUM_CASCADES] = m_CascadeShadowMap->CascadeEnds(NUM_CASCADES);
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
	m_GBufferAmbient.Resize(m_DxContext->GetDevice(), m_Width, m_Height);
	m_GBufferMetalness.Resize(m_DxContext->GetDevice(), m_Width, m_Height);
	m_GBufferRoughness.Resize(m_DxContext->GetDevice(), m_Width, m_Height);
	m_GBufferNormal.Resize(m_DxContext->GetDevice(), m_Width, m_Height);

	m_VelocityBuffer.Resize(m_DxContext->GetDevice(), m_Width, m_Height);
}