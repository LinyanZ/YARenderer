#include "pch.h"
#include "EnvironmentMap.h"
#include "RenderingUtils.h"

void EnvironmentMap::Load(const std::string &filename)
{
	auto device = m_DxContext->GetDevice();
	auto commandList = m_DxContext->GetCommandList();

	Texture equirectTex = Texture::Create(device, commandList,
										  Image::FromFile(filename), DXGI_FORMAT_R32G32B32A32_FLOAT, 1);

	m_DxContext->ExecuteCommandList();
	m_DxContext->Flush();

	m_EnvMap = RenderingUtils::Equirect2Cubemap(m_DxContext, equirectTex);
	RenderingUtils::GenerateMipmaps(m_DxContext, m_EnvMap);
	m_IrMap = RenderingUtils::ComputeDiffuseIrradianceCubemap(m_DxContext, m_EnvMap);
	m_SpMap = RenderingUtils::ComputePrefilteredSpecularEnvironmentMap(m_DxContext, m_EnvMap);
	m_BRDFLUT = RenderingUtils::ComputeBRDFLookUpTable(m_DxContext);
}