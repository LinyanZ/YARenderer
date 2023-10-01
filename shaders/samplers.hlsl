#ifndef __SAMPLERS_HLSL__
#define __SAMPLERS_HLSL__

SamplerState g_SamplerPointWrap : register(s0);
SamplerState g_SamplerPointClamp : register(s1);
SamplerState g_SamplerLinearWrap : register(s2);
SamplerState g_SamplerLinearClamp : register(s3);
SamplerState g_SamplerAnisotropicWrap : register(s4);
SamplerState g_SamplerAnisotropicClamp : register(s5);
SamplerComparisonState g_SamplerShadow : register(s6);

#endif // __SAMPLERS_HLSL__
