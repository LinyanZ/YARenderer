#pragma once

#include "dx/dx.h"
#include "dx/DxContext.h"
#include "dx/Texture.h"

class EnvironmentMap
{
public:
	EnvironmentMap(Ref<DxContext> dxContext) : m_DxContext(dxContext) {}
	void Load(const std::string& filename);

	Texture& GetEnvMap() { return m_EnvMap; }
	Texture& GetIrMap() { return m_IrMap; }
	Texture& GetSpMap() { return m_SpMap; }
	Texture& GetBRDFLUT() { return m_BRDFLUT; }

private:
	Ref<DxContext> m_DxContext;

	Texture m_EnvMap;
	Texture m_IrMap;	// Irradiance Cubemap
	Texture m_SpMap;	// Pre-filtered Specular Cubemap
	Texture m_BRDFLUT;  // BRDF Look-up Table
};