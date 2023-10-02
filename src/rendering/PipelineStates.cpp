#include "PipelineStates.h"
#include "dx/Utils.h"

RootSignature PipelineStates::m_RootSignature = nullptr;
std::unordered_map<std::string, PipelineState> PipelineStates::m_PSOs;

void PipelineStates::Init(Device device)
{
    BuildRootSignature(device);
    BuildPSOs(device);
}

void PipelineStates::Cleanup()
{
    m_RootSignature = nullptr;
    m_PSOs.clear();
}

void PipelineStates::BuildRootSignature(Device device)
{
    UINT MaxNumConstants = 32; // temporary limit

    auto staticSamplers = GetStaticSamplers();

    CD3DX12_ROOT_PARAMETER slotRootParameter[(UINT)RootParam::Count];
    slotRootParameter[(UINT)RootParam::ObjectCB].InitAsConstantBufferView((UINT)RootParam::ObjectCB);
    slotRootParameter[(UINT)RootParam::PassCB].InitAsConstantBufferView((UINT)RootParam::PassCB);
    slotRootParameter[(UINT)RootParam::MatCB].InitAsConstantBufferView((UINT)RootParam::MatCB);
    slotRootParameter[(UINT)RootParam::LightCB].InitAsConstantBufferView((UINT)RootParam::LightCB);
    slotRootParameter[(UINT)RootParam::ShadowCB].InitAsConstantBufferView((UINT)RootParam::ShadowCB);
    slotRootParameter[(UINT)RootParam::SSAOCB].InitAsConstantBufferView((UINT)RootParam::SSAOCB);
    slotRootParameter[(UINT)RootParam::RenderResources].InitAsConstants(MaxNumConstants, (UINT)RootParam::RenderResources);

    CD3DX12_ROOT_SIGNATURE_DESC desc((UINT)RootParam::Count, slotRootParameter,
                                     staticSamplers.size(), staticSamplers.data(),
                                     D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
                                         D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED |
                                         D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED);

    m_RootSignature = Utils::CreateRootSignature(device, desc);
}

void PipelineStates::BuildPSOs(Device device)
{
    std::vector<D3D12_INPUT_ELEMENT_DESC> defaultInputLayout =
        {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"BITANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 48, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        };

    std::vector<D3D12_INPUT_ELEMENT_DESC> skyBoxInputLayout =
        {{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};

    D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsDesc;
    ZeroMemory(&graphicsDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    graphicsDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    graphicsDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    graphicsDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    graphicsDesc.SampleMask = UINT_MAX;
    graphicsDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    graphicsDesc.NumRenderTargets = 1;
    graphicsDesc.RTVFormats[0] = BACK_BUFFER_FORMAT;
    graphicsDesc.SampleDesc = {1, 0};
    graphicsDesc.DSVFormat = DEPTH_STENCIL_FORMAT;
    graphicsDesc.pRootSignature = m_RootSignature.Get();

    // skybox PSO
    {
        Shader VS = Utils::CompileShader(L"shaders\\skybox.hlsl", nullptr, L"VS", L"vs_6_6");
        Shader PS = Utils::CompileShader(L"shaders\\skybox.hlsl", nullptr, L"PS", L"ps_6_6");

        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = graphicsDesc;
        desc.InputLayout = {skyBoxInputLayout.data(), (UINT)skyBoxInputLayout.size()};
        desc.VS = CD3DX12_SHADER_BYTECODE(VS->GetBufferPointer(), VS->GetBufferSize());
        desc.PS = CD3DX12_SHADER_BYTECODE(PS->GetBufferPointer(), PS->GetBufferSize());

        // camera is inside the skybox, so just turn off the culling
        desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

        // skybox is drawn with depth value of z = 1 (NDC), which will fail the depth test
        // if the depth buffer was cleared to 1 and the depth function is LESS
        desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

        ThrowIfFailed(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_PSOs["skybox"])));
    }

    // shadow pass
    {
        Shader VS = Utils::CompileShader(L"shaders\\shadow.hlsl", nullptr, L"VS", L"vs_6_6");

        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = graphicsDesc;
        desc.InputLayout = {defaultInputLayout.data(), (UINT)defaultInputLayout.size()};
        desc.VS = CD3DX12_SHADER_BYTECODE(VS->GetBufferPointer(), VS->GetBufferSize());

        desc.RasterizerState.DepthBias = 100000;
        desc.RasterizerState.DepthBiasClamp = 10.0f;
        desc.RasterizerState.SlopeScaledDepthBias = 1.0f;

        desc.NumRenderTargets = 0;
        desc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;

        ThrowIfFailed(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_PSOs["shadow"])));
    }

    // gbuffer pass
    {
        Shader VS = Utils::CompileShader(L"shaders\\gbuffer.hlsl", nullptr, L"VS", L"vs_6_6");
        Shader PS = Utils::CompileShader(L"shaders\\gbuffer.hlsl", nullptr, L"PS", L"ps_6_6");

        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = graphicsDesc;
        desc.InputLayout = {defaultInputLayout.data(), (UINT)defaultInputLayout.size()};
        desc.VS = CD3DX12_SHADER_BYTECODE(VS->GetBufferPointer(), VS->GetBufferSize());
        desc.PS = CD3DX12_SHADER_BYTECODE(PS->GetBufferPointer(), PS->GetBufferSize());

        desc.NumRenderTargets = 6;
        desc.RTVFormats[0] = GBUFFER_ALBEDO_FORMAT;
        desc.RTVFormats[1] = GBUFFER_NORMAL_FORMAT;
        desc.RTVFormats[2] = GBUFFER_METALNESS_FORMAT;
        desc.RTVFormats[3] = GBUFFER_ROUGHNESS_FORMAT;
        desc.RTVFormats[4] = GBUFFER_AMBIENT_FORMAT;
        desc.RTVFormats[5] = GBUFFER_VELOCITY_FORMAT;

        ThrowIfFailed(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_PSOs["gbuffer"])));
    }

    // deferred lighting pass
    {
        Shader VS = Utils::CompileShader(L"shaders\\deferredLighting.hlsl", nullptr, L"VS", L"vs_6_6");
        Shader PS = Utils::CompileShader(L"shaders\\deferredLighting.hlsl", nullptr, L"PS", L"ps_6_6");

        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = graphicsDesc;
        desc.VS = CD3DX12_SHADER_BYTECODE(VS->GetBufferPointer(), VS->GetBufferSize());
        desc.PS = CD3DX12_SHADER_BYTECODE(PS->GetBufferPointer(), PS->GetBufferSize());

        desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        desc.DepthStencilState.DepthEnable = false;

        ThrowIfFailed(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_PSOs["deferredLighting"])));
    }

    // clear voxel
    {
        Shader CS = Utils::CompileShader(L"shaders\\clearVoxel.hlsl", nullptr, L"main", L"cs_6_6");

        D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
        desc.pRootSignature = m_RootSignature.Get();
        desc.CS = CD3DX12_SHADER_BYTECODE(CS->GetBufferPointer(), CS->GetBufferSize());
        ThrowIfFailed(device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&m_PSOs["clearVoxel"])));
    }

    // voxelize
    {
        Shader VS = Utils::CompileShader(L"shaders\\voxelize.hlsl", nullptr, L"VS", L"vs_6_6");
        Shader GS = Utils::CompileShader(L"shaders\\voxelize.hlsl", nullptr, L"GS", L"gs_6_6");
        Shader PS = Utils::CompileShader(L"shaders\\voxelize.hlsl", nullptr, L"PS", L"ps_6_6");

        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = graphicsDesc;
        desc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON;
        desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        desc.DepthStencilState.DepthEnable = false;
        desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
        desc.NumRenderTargets = 0;
        desc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
        desc.InputLayout = {defaultInputLayout.data(), (UINT)defaultInputLayout.size()};
        desc.VS = CD3DX12_SHADER_BYTECODE(VS->GetBufferPointer(), VS->GetBufferSize());
        desc.GS = CD3DX12_SHADER_BYTECODE(GS->GetBufferPointer(), GS->GetBufferSize());
        desc.PS = CD3DX12_SHADER_BYTECODE(PS->GetBufferPointer(), PS->GetBufferSize());

        ThrowIfFailed(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_PSOs["voxelize"])));
    }

    // voxel buffer to texture 3d
    {
        Shader CS = Utils::CompileShader(L"shaders\\voxelBuffer2Tex.hlsl", nullptr, L"main", L"cs_6_6");

        D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
        desc.pRootSignature = m_RootSignature.Get();
        desc.CS = CD3DX12_SHADER_BYTECODE(CS->GetBufferPointer(), CS->GetBufferSize());
        ThrowIfFailed(device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&m_PSOs["voxelBuffer2Tex"])));
    }

    // voxel debug
    {
        Shader VS = Utils::CompileShader(L"shaders\\voxelDebug.hlsl", nullptr, L"VS", L"vs_6_6");
        Shader GS = Utils::CompileShader(L"shaders\\voxelDebug.hlsl", nullptr, L"GS", L"gs_6_6");
        Shader PS = Utils::CompileShader(L"shaders\\voxelDebug.hlsl", nullptr, L"PS", L"ps_6_6");

        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = graphicsDesc;
        desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        // desc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
        desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
        desc.VS = CD3DX12_SHADER_BYTECODE(VS->GetBufferPointer(), VS->GetBufferSize());
        desc.GS = CD3DX12_SHADER_BYTECODE(GS->GetBufferPointer(), GS->GetBufferSize());
        desc.PS = CD3DX12_SHADER_BYTECODE(PS->GetBufferPointer(), PS->GetBufferSize());

        ThrowIfFailed(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_PSOs["voxelDebug"])));
    }

    // generate voxel mipmap
    {
        Shader CS = Utils::CompileShader(L"shaders\\voxelMipmap.hlsl", nullptr, L"main", L"cs_6_6");

        D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
        desc.pRootSignature = m_RootSignature.Get();
        desc.CS = CD3DX12_SHADER_BYTECODE(CS->GetBufferPointer(), CS->GetBufferSize());
        ThrowIfFailed(device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&m_PSOs["voxelMipmap"])));
    }
}

std::vector<CD3DX12_STATIC_SAMPLER_DESC> PipelineStates::GetStaticSamplers()
{
    const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
        0,                                // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_POINT,   // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
        1,                                 // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_POINT,    // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
        2,                                // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_LINEAR,  // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
        3,                                 // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_LINEAR,   // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
        4,                                // shaderRegister
        D3D12_FILTER_ANISOTROPIC,         // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
        5,                                 // shaderRegister
        D3D12_FILTER_ANISOTROPIC,          // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

    CD3DX12_STATIC_SAMPLER_DESC shadow(
        6,                                         // shaderRegister
        D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT, // filter
        D3D12_TEXTURE_ADDRESS_MODE_BORDER,         // addressU
        D3D12_TEXTURE_ADDRESS_MODE_BORDER,         // addressV
        D3D12_TEXTURE_ADDRESS_MODE_BORDER,         // addressW
        0.0f,                                      // mipLODBias
        16,                                        // maxAnisotropy
        D3D12_COMPARISON_FUNC_LESS_EQUAL,
        D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK);

    return {pointWrap, pointClamp,
            linearWrap, linearClamp,
            anisotropicWrap, anisotropicClamp,
            shadow};
}
