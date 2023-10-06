#include "pch.h"
#include "dx/dx.h"
#include "dx/DxContext.h"
#include "dx/Descriptor.h"

#define VOXEL_DIMENSION 256

struct Voxel
{
    UINT64 Radiance;
    UINT64 Normal;
};

class VXGI
{
public:
    VXGI(Ref<DxContext> dxContext, UINT size);
    void OnResize(UINT x, UINT y, UINT z);

    D3D12_VIEWPORT &GetViewPort() { return m_ViewPort; }
    D3D12_RECT &GetScissorRect() { return m_ScissorRect; }

    Descriptor &GetVoxelBufferUav() { return m_VoxelBufferUav; }
    Descriptor &GetTextureSrv(int i) { return m_TextureSrv[i]; }

    void BufferToTexture3D(GraphicsCommandList commandList);

private:
    void GenVoxelMipmap(GraphicsCommandList commandList, int index);
    void ComputeSecondBound(GraphicsCommandList commandList);

    void BuildResources();
    void BuildDescriptors();

private:
    Device m_Device;

    Resource m_VoxelBuffer = nullptr;
    Descriptor m_VoxelBufferUav;

    Resource m_VolumeTexture[2];
    DXGI_FORMAT m_TextureFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
    UINT m_MipLevels = 0;

    Descriptor m_TextureSrv[2];
    std::vector<Descriptor> m_TextureUav;

    UINT m_X = -1, m_Y = -1, m_Z = -1;
    D3D12_VIEWPORT m_ViewPort;
    D3D12_RECT m_ScissorRect;
};