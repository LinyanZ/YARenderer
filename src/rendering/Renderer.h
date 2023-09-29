#pragma once

#include "pch.h"

#include "core/Window.h"
#include "core/Timer.h"
#include "core/MathHelper.h"

#include "dx/DxContext.h"
#include "dx/Utils.h"

#include "FrameResource.h"
#include "PipelineStateManager.h"
#include "Camera.h"
#include "SSAO.h"
#include "postProcessing/PostProcessing.h"
#include "Material.h"
#include "Light.h"
#include "Mesh.h"
#include "CascadeShadowMap.h"
#include "EnvironmentMap.h"
#include "RenderingUtils.h"
#include "TAA.h"

#include "vxgi/VolumeTexture.h"

#define SPONZA_SCENE 0
#define TEST_SCENE (!SPONZA_SCENE)

#define FORWARD_RENDERING 1
#define DEFERRED_RENDERING 0
#define FORWARD_PLUS_RENDERING 0

#define VOXEL_DIMENSION 512

enum class RenderPass
{
	NormalOnly = 0,
	Shadow,
	ForwardRendering,
	Velocity
};

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
	~Renderer() { RenderingUtils::Cleanup(); }

	void Setup();
	void OnUpdate(Timer& timer);

	void BeginFrame();
	void Render();
	void EndFrame();

	void OnResize(UINT width, UINT height);
	void ResizeResources();

	void OnKeyboardInput(float dt);
	void OnMouseInput(int dxPixel, int dyPixel);

	Light& GetDirectionalLight() { return m_Lights[0]; }

private:
	void BuildResources();
	void AllocateDescriptors();
	void BuildDescriptors();

	FrameResource* CurrFrameResource() { return m_FrameResources[m_CurrFrameResourceIndex].get(); }

	void UpdateLights(Timer& timer);
	void UpdateObjectConstantBuffers();
	void UpdateMainPassConstantBuffer(Timer& timer);
	void UpdateMaterialConstantBuffer();
	void UpdateSSAOConstantBuffer();
	void UpdateShadowPassCB();

	void BuildLightingDataBuffer();
	void BuildRenderItems();

	void ForwardRendering(GraphicsCommandList commandList);

	void DeferredRendering(GraphicsCommandList commandList);
	void GBufferPass(GraphicsCommandList commandList);
	void AmbientLightPass(GraphicsCommandList commandList);
	void LightingPass(GraphicsCommandList commandList);

	void DrawNormalsAndDepth(GraphicsCommandList commandList);
	void ShadowMapPass(GraphicsCommandList commandList);
	void DrawRenderItems(GraphicsCommandList commandList, RenderPass pass = RenderPass::ForwardRendering, bool transparent = false);
	void DrawSkybox(GraphicsCommandList commandList);
	void VelocityBufferPass(GraphicsCommandList commandList);

	void ClearVoxel(GraphicsCommandList commandList);
	void VoxelizeScene(GraphicsCommandList commandList);
	void DebugVoxel(GraphicsCommandList commandList);
	void VoxelComputeAverage(GraphicsCommandList commandList);
	void GenVoxelMipmap(GraphicsCommandList commandList);

	void Debug(GraphicsCommandList commandList, Descriptor srv, int slot = 0);

private:
	Ref<DxContext> m_DxContext;

	std::vector<std::unique_ptr<FrameResource>> m_FrameResources;
	int m_CurrFrameResourceIndex = 0;

	std::unique_ptr<PipelineStateManager> m_PipelineStateManager;

	D3D12_VIEWPORT m_ScreenViewport = {};
	D3D12_RECT m_ScissorRect = {};

	UINT m_Width;
	UINT m_Height;

	PassConstants m_MainPassCB;
	ShadowPassConstants m_ShadowPassCB;

	Camera m_Camera;
	std::unique_ptr<CascadeShadowMap> m_CascadeShadowMap;

	Ref<Mesh> m_PointLightGeo;
	Ref<Mesh> m_Skybox;
	std::vector<Ref<RenderItem>> m_RenderItems;

	std::unique_ptr<SSAO> m_SSAO;
	std::unique_ptr<EnvironmentMap> m_EnvironmentMap;
	std::unique_ptr<TAA> m_TAA;
	std::unique_ptr<PostProcessing> m_PostProcessing;

	std::vector<Light> m_Lights;

	Texture m_VelocityBuffer;
	XMMATRIX m_PrevViewProjMatrix;
	UINT m_JitterIndex = 0;
	float m_PreviousJitterX = 0;
	float m_PreviousJitterY = 0;

	bool m_FirstFrame = true;

	//
	// Defered rendering
	//

	Texture m_GBufferAlbedo;
	Texture m_GBufferNormal;
	Texture m_GBufferMetalness;
	Texture m_GBufferRoughness;
	Texture m_GBufferAmbient;

	// VXGI
	VolumeTexture m_VolumeAlbedo;
	bool temp = false;
};