#include "VXGI.h"
#include "PipelineStates.h"

VXGI::VXGI(Ref<DxContext> dxContext, UINT size)
{
    m_Device = dxContext->GetDevice();
    m_ViewPort = {0.0f, 0.0f, (float)size, (float)size, 0.0f, 1.0f};
    m_ScissorRect = {0, 0, (int)size, (int)size};

    m_MipLevels = 1;
    while (size >> m_MipLevels)
        m_MipLevels++;
    m_TextureUav.resize(m_MipLevels);

    // allocate descriptors
    m_TextureSrv = dxContext->GetCbvSrvUavHeap().Alloc();
    for (int i = 0; i < m_MipLevels; i++)
        m_TextureUav[i] = dxContext->GetCbvSrvUavHeap().Alloc();

    m_VoxelBufferUav = dxContext->GetCbvSrvUavHeap().Alloc();

    // build resources and descriptors
    OnResize(size, size, size);

    // initialize the voxel buffer to zero
    auto commandList = dxContext->GetCommandList();

    ID3D12DescriptorHeap *descriptorHeaps[] = {dxContext->GetCbvSrvUavHeap().Get()};
    commandList->SetDescriptorHeaps(1, descriptorHeaps);

    commandList->SetPipelineState(PipelineStates::GetPSO("clearVoxel"));
    commandList->SetComputeRootSignature(PipelineStates::GetRootSignature());
    commandList->SetComputeRoot32BitConstants((UINT)RootParam::RenderResources, 1, &m_VoxelBufferUav.Index, 0);

    UINT groupSize = size / 8;
    commandList->Dispatch(groupSize, groupSize, groupSize);

    dxContext->ExecuteCommandList();
    dxContext->Flush();
}

void VXGI::OnResize(UINT x, UINT y, UINT z)
{
    if (m_X == x && m_Y == y && m_Z == z)
        return;

    m_X = x;
    m_Y = y;
    m_Z = z;

    BuildResources();
    BuildDescriptors();
}

void VXGI::BufferToTexture3D(GraphicsCommandList commandList)
{
    UINT resources[2] = {m_VoxelBufferUav.Index,
                         m_TextureUav[0].Index};

    commandList->SetPipelineState(PipelineStates::GetPSO("voxelBuffer2Tex"));
    commandList->SetComputeRoot32BitConstants((UINT)RootParam::RenderResources, 2, resources, 0);

    UINT groupSize = std::max(VOXEL_DIMENSION / 8, 1);
    commandList->Dispatch(groupSize, groupSize, groupSize);
}

void VXGI::GenVoxelMipmap(GraphicsCommandList commandList)
{
    commandList->SetPipelineState(PipelineStates::GetPSO("voxelMipmap"));

    for (int i = 1, levelWidth = VOXEL_DIMENSION / 2; i < m_MipLevels; i++, levelWidth /= 2)
    {
        UINT resources[] = {m_TextureUav[i - 1].Index, m_TextureUav[i].Index};
        commandList->SetComputeRoot32BitConstants((UINT)RootParam::RenderResources, 2, resources, 0);

        UINT count = std::max(levelWidth / 8, 1);
        commandList->Dispatch(count, count, count);
    }
}

void VXGI::BuildResources()
{
    m_VolumeTexture = nullptr;

    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
    texDesc.Alignment = 0;
    texDesc.Width = m_X;
    texDesc.Height = m_Y;
    texDesc.DepthOrArraySize = m_Z;
    texDesc.MipLevels = m_MipLevels;
    texDesc.Format = m_TextureFormat;
    texDesc.SampleDesc = {1, 0};
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    ThrowIfFailed(m_Device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nullptr,
        IID_PPV_ARGS(m_VolumeTexture.GetAddressOf())));

    UINT byteSize = (m_X * m_Y * m_Z) * sizeof(Voxel);
    ThrowIfFailed(m_Device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(byteSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&m_VoxelBuffer)));
}

void VXGI::BuildDescriptors()
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = m_TextureFormat;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
    srvDesc.Texture3D.MostDetailedMip = 0;
    srvDesc.Texture3D.MipLevels = m_MipLevels;
    srvDesc.Texture3D.ResourceMinLODClamp = 0.0f;

    m_Device->CreateShaderResourceView(m_VolumeTexture.Get(), &srvDesc, m_TextureSrv.CPUHandle);

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = m_TextureFormat;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
    uavDesc.Texture3D.FirstWSlice = 0;
    uavDesc.Texture3D.WSize = -1;
    for (int i = 0; i < m_MipLevels; i++)
    {
        uavDesc.Texture3D.MipSlice = i;
        m_Device->CreateUnorderedAccessView(m_VolumeTexture.Get(), nullptr, &uavDesc, m_TextureUav[i].CPUHandle);
    }

    D3D12_UNORDERED_ACCESS_VIEW_DESC voxelUavDesc{};
    voxelUavDesc.Format = DXGI_FORMAT_UNKNOWN;
    voxelUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    voxelUavDesc.Buffer.FirstElement = 0;
    voxelUavDesc.Buffer.NumElements = m_X * m_Y * m_Z;
    voxelUavDesc.Buffer.StructureByteStride = sizeof(Voxel);
    voxelUavDesc.Buffer.CounterOffsetInBytes = 0;
    voxelUavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

    m_Device->CreateUnorderedAccessView(m_VoxelBuffer.Get(), nullptr, &voxelUavDesc, m_VoxelBufferUav.CPUHandle);
}
