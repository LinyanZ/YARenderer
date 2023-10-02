#include "pch.h"
#include "dx/dx.h"
#include "dx/DxContext.h"
#include "dx/Descriptor.h"

#define VOXEL_DIMENSION 128

struct Voxel
{
    UINT64 Radiance;
};

class VXGI
{
public:
    VXGI(Ref<DxContext> dxContext, UINT size);
    void OnResize(UINT x, UINT y, UINT z);

    D3D12_VIEWPORT &GetViewPort() { return m_ViewPort; }
    D3D12_RECT &GetScissorRect() { return m_ScissorRect; }

    Descriptor &GetVoxelBufferUav() { return m_VoxelBufferUav; }
    Descriptor &GetTextureSrv() { return m_TextureSrv; }

    void BufferToTexture3D(GraphicsCommandList commandList);
    void GenVoxelMipmap(GraphicsCommandList commandList);

private:
    void BuildResources();
    void BuildDescriptors();

private:
    Device m_Device;

    Resource m_VoxelBuffer = nullptr;
    Descriptor m_VoxelBufferUav;

    Resource m_VolumeTexture = nullptr;
    DXGI_FORMAT m_TextureFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
    UINT m_MipLevels = 0;

    Descriptor m_TextureSrv;
    std::vector<Descriptor> m_TextureUav;

    UINT m_X = -1, m_Y = -1, m_Z = -1;
    D3D12_VIEWPORT m_ViewPort;
    D3D12_RECT m_ScissorRect;
};