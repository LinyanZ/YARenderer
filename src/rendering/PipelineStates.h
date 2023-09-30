#pragma once

#include "dx/dx.h"
#include "pch.h"

enum struct RootParam : UINT
{
    ObjectCB = 0,
    PassCB,
    MatCB,
    LightCB,
    ShadowCB,
    SSAOCB,
    RenderResources,
    Count
};

// struct RootParamIndex
// {
//     static const UINT ObjectCB = 0;
//     static const UINT PassCB = 1;
//     static const UINT MatCB = 2;
//     static const UINT LightCB = 3;
//     static const UINT ShadowCB = 4;
//     static const UINT SSAOCB = 5;
//     static const UINT RenderResources = 6;
//     static const UINT Count = 7;
// };

class PipelineStates
{
public:
    static void Init(Device device);
    static void Cleanup();

    static ID3D12RootSignature *GetRootSignature() { return m_RootSignature.Get(); }
    static ID3D12PipelineState *GetPSO(const std::string &name) { return m_PSOs[name].Get(); }

private:
    static void BuildRootSignature(Device device);
    static void BuildPSOs(Device device);
    static std::vector<CD3DX12_STATIC_SAMPLER_DESC> GetStaticSamplers();

private:
    static RootSignature m_RootSignature;
    static std::unordered_map<std::string, PipelineState> m_PSOs;
};