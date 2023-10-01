#ifndef __RENDER_RESOURCES_HLSL__
#define __RENDER_RESOURCES_HLSL__

struct SkyBoxRenderResources
{
    uint EnvMapTexIndex;
};

struct ShadowRenderResources
{
    uint CascadeIndex;
};

struct DeferredLightingRenderResources
{
	uint AlbedoTexIndex;
	uint NormalTexIndex;
	uint MetalnessTexIndex;
	uint RoughnessTexIndex;
	uint AmbientTexIndex;
	uint DepthTexIndex;
};

#endif // __RENDER_RESOURCES_HLSL__