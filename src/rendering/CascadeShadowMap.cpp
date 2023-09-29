#include "pch.h"
#include "CascadeShadowMap.h"
#include "RenderingSettings.h"

extern RenderingSettings g_RenderingSettings;

CascadeShadowMap::CascadeShadowMap(Ref<DxContext> dxContext)
	: m_Device(dxContext->GetDevice())
{
	D3D12_RESOURCE_DESC texDesc = {};
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = SHADOW_MAP_SIZE;
	texDesc.Height = SHADOW_MAP_SIZE;
	texDesc.DepthOrArraySize = 4;
	texDesc.MipLevels = 1;
	texDesc.Format = m_Format;
	texDesc.SampleDesc = { 1, 0 };
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE optClear = {};
	optClear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;

	ThrowIfFailed(m_Device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		&optClear,
		IID_PPV_ARGS(&m_Resource)));

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

	dsvDesc.Texture2DArray.ArraySize = 1;
	dsvDesc.Texture2DArray.MipSlice = 0;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
	srvDesc.Texture2DArray.MostDetailedMip = 0;
	srvDesc.Texture2DArray.MipLevels = 1;
	srvDesc.Texture2DArray.ArraySize = 1;

	for (int i = 0; i < 4; i++)
	{
		m_Dsvs[i] = dxContext->GetDsvHeap().Alloc();
		dsvDesc.Texture2DArray.FirstArraySlice = i;
		m_Device->CreateDepthStencilView(m_Resource.Get(), &dsvDesc, m_Dsvs[i].CPUHandle);

		m_Srvs[i] = dxContext->GetCbvSrvUavHeap().Alloc();
		srvDesc.Texture2DArray.FirstArraySlice = i;
		m_Device->CreateShaderResourceView(m_Resource.Get(), &srvDesc, m_Srvs[i].CPUHandle);
	}

	m_Srvs[4] = dxContext->GetCbvSrvUavHeap().Alloc();
	srvDesc.Texture2DArray.FirstArraySlice = 0;
	srvDesc.Texture2DArray.ArraySize = 4;
	m_Device->CreateShaderResourceView(m_Resource.Get(), &srvDesc, m_Srvs[4].CPUHandle);
}

void CascadeShadowMap::CalcOrthoProjs(const Camera& camera, const Light& mainLight)
{
	// Calculate each cascade's range
	m_CascadeEnds[0] = camera.GetNearZ();

	float expScale[NUM_CASCADES] = { 1.0f };
	float expNormalizeFactor = 1.0f;
	for (int i = 1; i < NUM_CASCADES; i++)
	{
		expScale[i] = expScale[i - 1] * g_RenderingSettings.CascadeRangeScale;
		expNormalizeFactor += expScale[i];
	}
	expNormalizeFactor = 1.0f / expNormalizeFactor;

	float percentage = 0.0f;
	for (int i = 0; i < NUM_CASCADES; i++)
	{
		float percentageOffset = expScale[i] * expNormalizeFactor;
		percentage += percentageOffset;
		m_CascadeEnds[i + 1] = percentage * g_RenderingSettings.MaxShadowDistance;
	}

	XMMATRIX view = camera.GetView();

	// Only the first "main" light casts a shadow.
	XMVECTOR lightDir = XMLoadFloat4(&mainLight.DirectionWS);
	XMVECTOR lightUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	for (int i = 0; i < NUM_CASCADES; i++)
	{
		float nearZ = m_CascadeEnds[i];
		float farZ = m_CascadeEnds[i + 1];

		if (i > 0)
		{
			float cascadeLength = m_CascadeEnds[i] - m_CascadeEnds[i - 1];
			nearZ -= cascadeLength * g_RenderingSettings.CascadeTransitionRatio;
		}

		// Get frustum corner coordinates in world space
		XMMATRIX proj = XMMatrixPerspectiveFovLH(camera.GetFovY(), camera.GetAspect(), nearZ, farZ);
		XMMATRIX viewProj = XMMatrixMultiply(view, proj);
		XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

		std::vector<XMVECTOR> frustumCornersWS;

		for (int x = 0; x < 2; x++)
		{
			for (int y = 0; y < 2; y++)
			{
				for (int z = 0; z < 2; z++)
				{
					XMVECTOR NDCCoords = XMVectorSet(2.0f * x - 1.0f, 2.0f * y - 1.0f, z, 1.0f);
					auto world = XMVector4Transform(NDCCoords, invViewProj);
					float w = XMVectorGetW(world);
					frustumCornersWS.push_back(world / w);
				}
			}
		}

		// Calculate the bounding sphere
		// ref: https://zhuanlan.zhihu.com/p/515385379
		auto nearPlaneDiagonal = frustumCornersWS[6] - frustumCornersWS[0];
		float a2 = XMVectorGetX(XMVector3LengthSq(nearPlaneDiagonal));

		auto farPlaneDiagonal = frustumCornersWS[7] - frustumCornersWS[1];
		float b2 = XMVectorGetX(XMVector3LengthSq(farPlaneDiagonal));

		float len = farZ - nearZ;
		float x = len * 0.5f - (a2 - b2) / (8.0f * len);

		auto sphereCenterWS = camera.GetPosition() + camera.GetLook() * (nearZ + x);
		float sphereRadius = sqrtf(x * x + a2 * 0.25);
		//LOG_INFO("Cascade radius [{}]: {}", i, sphereRadius);

		// for removing edge shimmer effect
		XMMATRIX shadowView = XMMatrixLookAtLH(XMVectorSet(0, 0, 0, 1), -lightDir, lightUp);
		XMMATRIX invShadowView = XMMatrixInverse(&XMMatrixDeterminant(shadowView), shadowView);

		float worldUnitsPerTexel = sphereRadius * 2.0 / SHADOW_MAP_SIZE;
		auto vWorldUnitsPerTexel = XMVectorSet(worldUnitsPerTexel, worldUnitsPerTexel, 1, 1);

		auto sphereCenterLS = XMVector3Transform(sphereCenterWS, shadowView);
		sphereCenterLS /= vWorldUnitsPerTexel;
		sphereCenterLS = XMVectorFloor(sphereCenterLS);
		sphereCenterLS *= vWorldUnitsPerTexel;
		sphereCenterWS = XMVector3Transform(sphereCenterLS, invShadowView);

		float sceneRadius = 50.0f; // TODO: use an actual bounding sphere?
		float backDistance = sceneRadius + XMVectorGetX(XMVector3Length(sphereCenterWS));

		XMMATRIX lightView = XMMatrixLookAtLH(sphereCenterWS - lightDir * backDistance, sphereCenterWS, lightUp);
		XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(-sphereRadius, sphereRadius, -sphereRadius, sphereRadius, 0, backDistance * 2);
		m_ViewProjMatrix[i] = lightView * lightProj;
		m_CascadeRadius[i] = sphereRadius;
	}
}