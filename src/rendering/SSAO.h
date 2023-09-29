#pragma once

#include "dx/DxContext.h"
#include "FrameResource.h"

class SSAO
{
public:
	SSAO(Ref<DxContext> dxContext, UINT width, UINT height);

	static const int MaxBlurRadius = 5;

	UINT SSAOMapWidth()const { return m_RenderTargetWidth / 2; }
	UINT SSAOMapHeight()const { return m_RenderTargetHeight / 2; }

	std::vector<float> CalcGaussWeights(float sigma);

	void OnResize(UINT width, UINT height);

	///<summary>
	/// Changes the render target to the Ambient render target and draws a fullscreen
	/// quad to kick off the pixel shader to compute the AmbientMap.  We still keep the
	/// main depth buffer binded to the pipeline, but depth buffer read/writes
	/// are disabled, as we do not need the depth buffer computing the Ambient map.
	///</summary>
	void ComputeSSAO(
		GraphicsCommandList cmdList,
		Texture normalMap, Descriptor depthMapSrv,
		FrameResource* currFrame,
		int blurCount);

public:
	Texture& AmbientMap() { return m_AmbientMap0; }

private:
	void AllocateDescriptors(DescriptorHeap& GetRtvHeap, DescriptorHeap& SrvHeap);

	///<summary>
	/// Blurs the ambient map to smooth out the noise caused by only taking a
	/// few random samples per pixel.  We use an edge preserving blur so that
	/// we do not blur across discontinuities--we want edges to remain edges.
	///</summary>
	void BlurAmbientMap(GraphicsCommandList cmdList, Texture normalMap, FrameResource* currFrame, int blurCount);
	void BlurAmbientMap(GraphicsCommandList cmdList, Texture normalMap, bool horzBlur);

	void BuildResources();
	void BuildDescriptors();
	void BuildRootSignature();
	void BuildPSOs();

private:
	Device m_Device;

	RootSignature m_SSAORootSig;
	PipelineState m_SSAOPso = nullptr;
	PipelineState m_BlurPso = nullptr;

	UINT m_RenderTargetWidth;
	UINT m_RenderTargetHeight;

	D3D12_VIEWPORT m_Viewport;
	D3D12_RECT m_ScissorRect;

	// Need two for ping-ponging during blur.
	Texture m_AmbientMap0;
	Texture m_AmbientMap1;
};
