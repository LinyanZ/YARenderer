#pragma once

#include "pch.h"

#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>
#include <assimp/Exporter.hpp>
#include <assimp/DefaultLogger.hpp>
#include <assimp/LogStream.hpp>

#include "Material.h"
#include "dx/dx.h"
#include "dx/DescriptorHeap.h"
#include "dx/Utils.h"

class Mesh
{
public:
	struct Vertex
	{
		XMFLOAT3 Position;
		XMFLOAT3 Normal;
		XMFLOAT3 Tangent;
		XMFLOAT3 Bitangent;
		XMFLOAT2 TexCoord;

		static const int NumAttributes = 5;
	};

	typedef UINT32 Index;

	struct SubMesh
	{
		UINT MaterialIndex;
		UINT IndexCount = 0;
		UINT StartIndexLocation = 0;
		INT BaseVertexLocation = 0;
		bool Transparent = false;
	};

public:
	static Ref<Mesh> FromFile(const std::string& filename);
	static Ref<Mesh> CreateSphere(float tesselation = 4);
	static Ref<Mesh> CreateCube();

	std::vector<Vertex>& Vertices() { return m_Vertices; }
	std::vector<Index>& Indices() { return m_Indices; }
	std::vector<SubMesh>& SubMeshes() { return m_SubMeshes; }
	std::vector<Material>& Materials() { return m_Materials; }

	D3D12_VERTEX_BUFFER_VIEW VertexBufferView() const { return m_VertexBufferView; }
	D3D12_INDEX_BUFFER_VIEW IndexBufferView() const { return m_IndexBufferView; }

	void UploadVertexAndIndexBufferToGPU(Device device, GraphicsCommandList commandList);
	void LoadTextures(Device device, GraphicsCommandList commandList, DescriptorHeap& srvHeap);

private:
	void InitFromScene(const aiScene* scene, const std::string& filename);
	void InitSubMesh(unsigned int index, const aiMesh* mesh, const aiMaterial* material);
	void InitMaterials(const aiScene* scene, const std::string& filename);

private:
	std::vector<SubMesh> m_SubMeshes;
	std::vector<Material> m_Materials;

	std::vector<Vertex> m_Vertices;
	std::vector<Index> m_Indices;

	Resource m_VertexBufferGPU = nullptr;
	Resource m_IndexBufferGPU = nullptr;

	Resource m_VertexBufferUploader = nullptr;
	Resource m_IndexBufferUploader = nullptr;

	D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView;
	D3D12_INDEX_BUFFER_VIEW m_IndexBufferView;
};