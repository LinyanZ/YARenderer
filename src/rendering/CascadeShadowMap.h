#pragma once

#include "pch.h"
#include "dx/DxContext.h"
#include "Camera.h"
#include "Light.h"

#define NUM_CASCADES 4
#define NUM_FRUSTUM_CORNERS 8
#define SHADOW_MAP_SIZE 4096

class CascadeShadowMap
{
public:
	CascadeShadowMap(Ref<DxContext> dxContext);

	void CalcOrthoProjs(const Camera& camera, const Light& mainLight);

	XMMATRIX ViewProjMatrix(int index) const { return m_ViewProjMatrix[index]; }
	float CascadeRadius(int index) const { return m_CascadeRadius[index]; }
	float CascadeEnds(int index) const { return m_CascadeEnds[index]; }

	ID3D12Resource* GetResource() { return m_Resource.Get(); }
	Descriptor& Dsv(int index) { return m_Dsvs[index]; }
	Descriptor& Srv(int index) { return m_Srvs[index]; }
	D3D12_VIEWPORT& Viewport() { return m_Viewport; }
	D3D12_RECT& ScissorRect() { return m_ScissorRect; }

private:
	Device m_Device;

	D3D12_VIEWPORT m_Viewport{ 0.0f, 0.0f, (float)SHADOW_MAP_SIZE, (float)SHADOW_MAP_SIZE, 0.0f, 1.0f };
	D3D12_RECT m_ScissorRect{ 0, 0, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE };

	DXGI_FORMAT m_Format = DXGI_FORMAT_R24G8_TYPELESS;

	Resource m_Resource;
	Descriptor m_Dsvs[4];
	Descriptor m_Srvs[5]; // 0-3 for each cascade depth, 4 for the whole resource

	XMMATRIX m_ViewProjMatrix[NUM_CASCADES];
	float m_CascadeRadius[NUM_CASCADES];
	float m_CascadeEnds[NUM_CASCADES + 1];
};