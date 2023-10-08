#pragma once

#include "pch.h"

#include "core/Window.h"
#include "core/Timer.h"
#include "core/MathHelper.h"

#include "dx/DxContext.h"
#include "dx/Utils.h"

#include "FrameResource.h"
#include "Camera.h"
#include "PostProcessing.h"
#include "Material.h"
#include "Light.h"
#include "Mesh.h"
#include "CascadedShadowMap.h"
#include "EnvironmentMap.h"
#include "RenderingUtils.h"
#include "PipelineStates.h"
#include "SSAO.h"
#include "TAA.h"
#include "VXGI.h"

#define SPONZA_SCENE 0
#define TEST_SCENE (!SPONZA_SCENE)

struct RenderItem
{
	XMFLOAT4X4 World = MathHelper::Identity4x4();

	int NumFramesDirty = NUM_FRAMES_IN_FLIGHT;
	UINT objCBIndex = -1;
	UINT matCBIndex = -1;

	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	Ref<Mesh> Mesh;
};

struct Renderer
{
public:
	Renderer(Ref<DxContext> dxContext, UINT width, UINT height);
	~Renderer()
	{
		PipelineStates::Cleanup();
	}

	void Setup();
	void OnUpdate(Timer &timer);

	void BeginFrame();
	void Render();
	void EndFrame();

	void OnResize(UINT width, UINT height);
	void ResizeResources();

	void OnKeyboardInput(float dt);
	void OnMouseInput(int dxPixel, int dyPixel);

	Light &GetDirectionalLight() { return m_Lights[0]; }

private:
	void BuildResources();
	void AllocateDescriptors();
	void BuildDescriptors();

	FrameResource *CurrFrameResource() { return m_FrameResources[m_CurrFrameResourceIndex].get(); }

	void UpdateLights(Timer &timer);
	void UpdateObjectConstantBuffers();
	void UpdateMainPassConstantBuffer(Timer &timer);
	void UpdateMaterialConstantBuffer();
	void UpdateSSAOConstantBuffer();
	void UpdateShadowPassCB();

	void BuildLightingDataBuffer();
	void BuildRenderItems();

	void GBufferPass(GraphicsCommandList commandList);
	void DeferredLightingPass(GraphicsCommandList commandList);

	void ShadowMapPass(GraphicsCommandList commandList);
	void DrawRenderItems(GraphicsCommandList commandList, bool transparent = false);
	void DrawSkybox(GraphicsCommandList commandList);

	void VoxelizeScene(GraphicsCommandList commandList);
	void DebugVoxel(GraphicsCommandList commandList);

	// divide the whole window into 4x4 grid, and draw the texture at slot <slot>
	void Debug(GraphicsCommandList commandList, Descriptor srv, UINT slot = 0);

private:
	Ref<DxContext> m_DxContext;

	std::vector<std::unique_ptr<FrameResource>> m_FrameResources;
	int m_CurrFrameResourceIndex = 0;

	D3D12_VIEWPORT m_ScreenViewport = {};
	D3D12_RECT m_ScissorRect = {};

	UINT m_Width;
	UINT m_Height;

	PassConstants m_MainPassCB;
	ShadowPassConstants m_ShadowPassCB;

	Camera m_Camera;
	std::unique_ptr<CascadedShadowMap> m_CascadedShadowMap;

	Ref<Mesh> m_Skybox;
	std::vector<Ref<RenderItem>> m_RenderItems;

	std::unique_ptr<SSAO> m_SSAO;
	std::unique_ptr<EnvironmentMap> m_EnvironmentMap;
	std::unique_ptr<TAA> m_TAA;
	std::unique_ptr<PostProcessing> m_PostProcessing;
	std::unique_ptr<VXGI> m_VXGI;

	std::vector<Light> m_Lights;

	Texture m_GBufferAlbedo;
	Texture m_GBufferNormal;
	Texture m_GBufferMetalness;
	Texture m_GBufferRoughness;
	Texture m_GBufferAmbient;
	Texture m_GBufferVelocity;

	XMMATRIX m_PrevViewProjMatrix;
	UINT m_JitterIndex = 0;
	float m_PreviousJitterX = 0;
	float m_PreviousJitterY = 0;
};