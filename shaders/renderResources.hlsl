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

#endif // __RENDER_RESOURCES_HLSL__