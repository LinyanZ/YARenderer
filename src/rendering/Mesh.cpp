#include "pch.h"
#include "Mesh.h"

#include <filesystem>
#include <assimp/GltfMaterial.h>

struct LogStream : public Assimp::LogStream
{
	static void initialize()
	{
		if (Assimp::DefaultLogger::isNullLogger()) {
			Assimp::DefaultLogger::create("", Assimp::Logger::VERBOSE);
			Assimp::DefaultLogger::get()->attachStream(new LogStream, Assimp::Logger::Err | Assimp::Logger::Warn);
		}
	}

	void write(const char* message) override
	{
		LOG_WARN("Assimp: {}", message);
	}
};

Ref<Mesh> Mesh::FromFile(const std::string& filename)
{
	LogStream::initialize();
	Ref<Mesh> mesh = make_ref<Mesh>();

	Assimp::Importer importer;

	std::filesystem::path path = filename;
	std::filesystem::path exportPath = path.replace_extension("assbin");

	const aiScene* scene = nullptr;

	if (std::filesystem::exists(exportPath) && std::filesystem::is_regular_file(exportPath))
	{
		LOG_INFO("Loading scene: {}", exportPath.string());
		scene = importer.ReadFile(exportPath.string(), 0);
	}
	else
	{
		unsigned int ImportFlags =
			aiProcess_CalcTangentSpace |
			aiProcess_Triangulate |
			aiProcess_PreTransformVertices |
			aiProcess_GenNormals |
			aiProcess_GenUVCoords |
			aiProcess_OptimizeMeshes |
			aiProcess_Debone |
			aiProcess_ConvertToLeftHanded |
			aiProcess_JoinIdenticalVertices |
			aiProcess_ValidateDataStructure;

		LOG_INFO("Loading scene: {}", filename);
		scene = importer.ReadFile(filename, ImportFlags);

		if (scene)
		{
			Assimp::Exporter exporter;
			LOG_INFO("Exporting processed scene: {}", exportPath.string());
			exporter.Export(scene, "assbin", exportPath.string(), 0);
		}
	}

	ASSERT(scene, "Error parsing {}: {}", filename.c_str(), importer.GetErrorString());

	mesh->InitFromScene(scene, filename);
	return mesh;
}

void Mesh::UploadVertexAndIndexBufferToGPU(Device device, GraphicsCommandList commandList)
{
	const UINT vbByteSize = m_Vertices.size() * sizeof(Mesh::Vertex);
	const UINT ibByteSize = m_Indices.size() * sizeof(Mesh::Index);

	m_VertexBufferGPU = Utils::CreateDefaultBuffer(
		device, commandList, m_Vertices.data(), vbByteSize, m_VertexBufferUploader);

	m_VertexBufferView.BufferLocation = m_VertexBufferGPU->GetGPUVirtualAddress();
	m_VertexBufferView.SizeInBytes = vbByteSize;
	m_VertexBufferView.StrideInBytes = sizeof(Mesh::Vertex);

	m_IndexBufferGPU = Utils::CreateDefaultBuffer(
		device, commandList, m_Indices.data(), ibByteSize, m_IndexBufferUploader);

	m_IndexBufferView.BufferLocation = m_IndexBufferGPU->GetGPUVirtualAddress();
	m_IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	m_IndexBufferView.SizeInBytes = ibByteSize;
}

void Mesh::LoadTextures(Device device, GraphicsCommandList commandList, DescriptorHeap& srvHeap)
{
	for (auto& material : m_Materials)
	{
		if (material.HasAlbedoTexture)
		{
			material.AlbedoTexture = Texture::Create(device, commandList, Image::FromFile(material.AlbedoFilename), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 1);
			material.AlbedoTexture.Srv = srvHeap.Alloc();
			material.AlbedoTexture.CreateSrv(device, D3D12_SRV_DIMENSION_TEXTURE2D, 0, 1);
		}

		if (material.HasNormalTexture)
		{
			material.NormalTexture = Texture::Create(device, commandList, Image::FromFile(material.NormalFilename), DXGI_FORMAT_R8G8B8A8_UNORM, 1);
			material.NormalTexture.Srv = srvHeap.Alloc();
			material.NormalTexture.CreateSrv(device, D3D12_SRV_DIMENSION_TEXTURE2D, 0, 1);
		}

		if (material.HasMetalnessTexture)
		{
			material.MetalnessTexture = Texture::Create(device, commandList, Image::FromFile(material.MetalnessFilename), DXGI_FORMAT_R8G8B8A8_UNORM, 1);
			material.MetalnessTexture.Srv = srvHeap.Alloc();
			material.MetalnessTexture.CreateSrv(device, D3D12_SRV_DIMENSION_TEXTURE2D, 0, 1);
		}

		if (material.HasRoughnessTexture)
		{
			material.RoughnessTexture = Texture::Create(device, commandList, Image::FromFile(material.RoughnessFilename), DXGI_FORMAT_R8G8B8A8_UNORM, 1);
			material.RoughnessTexture.Srv = srvHeap.Alloc();
			material.RoughnessTexture.CreateSrv(device, D3D12_SRV_DIMENSION_TEXTURE2D, 0, 1);
		}
	}
}

void Mesh::InitFromScene(const aiScene* scene, const std::string& filename)
{
	UINT32 numVertices = 0;
	UINT32 numIndices = 0;
	for (unsigned int i = 0; i < scene->mNumMeshes; i++)
	{
		numVertices += scene->mMeshes[i]->mNumVertices;
		numIndices += scene->mMeshes[i]->mNumFaces * 3;
	}

	m_Vertices.reserve(numVertices);
	m_Indices.reserve(numIndices);
	m_Materials.reserve(scene->mNumMaterials);

	for (unsigned int i = 0; i < scene->mNumMeshes; i++)
		InitSubMesh(i, scene->mMeshes[i], scene->mMaterials[scene->mMeshes[i]->mMaterialIndex]);
	InitMaterials(scene, filename);
}

void Mesh::InitSubMesh(unsigned int index, const aiMesh* mesh, const aiMaterial* material)
{
	ASSERT(mesh->HasPositions(), "Mesh does not have positions.");
	ASSERT(mesh->HasNormals(), "Mesh does not have normals.");

	SubMesh subMesh;
	subMesh.MaterialIndex = mesh->mMaterialIndex;
	subMesh.IndexCount = mesh->mNumFaces * 3;
	subMesh.StartIndexLocation = m_Indices.size();
	subMesh.BaseVertexLocation = m_Vertices.size();

	if (material)
	{
		aiString blendFunc;
		auto result = material->Get(AI_MATKEY_GLTF_ALPHAMODE, blendFunc);
		if (result == AI_SUCCESS && strcmp(blendFunc.C_Str(), "BLEND") == 0)
			subMesh.Transparent = true;
	}

	m_SubMeshes.push_back(subMesh);

	for (size_t i = 0; i < mesh->mNumVertices; i++)
	{
		Vertex vertex = {};

		vertex.Position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
		vertex.Normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };

		if (mesh->HasTangentsAndBitangents())
		{
			vertex.Tangent = { mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z };
			vertex.Bitangent = { mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z };
		}

		if (mesh->HasTextureCoords(0))
			vertex.TexCoord = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };

		m_Vertices.push_back(vertex);
	}

	for (size_t i = 0; i < mesh->mNumFaces; i++)
	{
		ASSERT(mesh->mFaces[i].mNumIndices == 3, "Face with number of vertices other than 3.");
		m_Indices.push_back(mesh->mFaces[i].mIndices[0]);
		m_Indices.push_back(mesh->mFaces[i].mIndices[1]);
		m_Indices.push_back(mesh->mFaces[i].mIndices[2]);
	}
}

std::string GetTextureByType(aiMaterial* material, aiTextureType type)
{
	aiString path;
	if (material->GetTextureCount(type) > 0 && material->GetTexture(type, 0, &path) == AI_SUCCESS)
	{
		std::string s = path.C_Str();
		return s;
		auto found = s.find_last_of("/\\");
		return s.substr(found + 1);
	}
	else
		return "";
}

void Mesh::InitMaterials(const aiScene* scene, const std::string& filename)
{
	std::filesystem::path path = filename;
	auto parentPath = path.parent_path().string();
	auto rootDir = path.root_directory();
	auto rootName = path.root_name();

	for (unsigned int i = 0; i < scene->mNumMaterials; i++)
	{
		Material material;

		aiString blendFunc;
		auto result = scene->mMaterials[i]->Get(AI_MATKEY_GLTF_ALPHAMODE, blendFunc);
		if (result == AI_SUCCESS && strcmp(blendFunc.C_Str(), "BLEND") == 0)
			material.Albedo.w = 0.35; // baseColorFactor":[1.0,1.0,1.0,0.350000024] from the original file

		auto albedoTexName = GetTextureByType(scene->mMaterials[i], aiTextureType_DIFFUSE);
		auto normalTexName = GetTextureByType(scene->mMaterials[i], aiTextureType_NORMALS);
		auto metalnessTexName = GetTextureByType(scene->mMaterials[i], aiTextureType_METALNESS);
		auto roughnessTexName = GetTextureByType(scene->mMaterials[i], aiTextureType_DIFFUSE_ROUGHNESS);

		material.HasAlbedoTexture = !albedoTexName.empty();
		material.HasNormalTexture = !normalTexName.empty();
		material.HasMetalnessTexture = !metalnessTexName.empty();
		material.HasRoughnessTexture = !roughnessTexName.empty();

		if (material.HasAlbedoTexture) material.AlbedoFilename = parentPath + "/" + albedoTexName;
		if (material.HasNormalTexture) material.NormalFilename = parentPath + "/" + normalTexName;
		if (material.HasMetalnessTexture) material.MetalnessFilename = parentPath + "/" + metalnessTexName;
		if (material.HasRoughnessTexture) material.RoughnessFilename = parentPath + "/" + roughnessTexName;

		m_Materials.push_back(material);
	}
}

Ref<Mesh> Mesh::CreateSphere(float tesselation)
{
	Ref<Mesh> mesh = make_ref<Mesh>();
	Assimp::Importer importer;

	unsigned int ImportFlags =
		aiProcess_CalcTangentSpace |
		aiProcess_Triangulate |
		aiProcess_PreTransformVertices |
		aiProcess_GenNormals |
		aiProcess_GenUVCoords |
		aiProcess_OptimizeMeshes |
		aiProcess_Debone |
		aiProcess_ConvertToLeftHanded |
		aiProcess_JoinIdenticalVertices |
		aiProcess_ValidateDataStructure;

	std::stringstream ss;

	ss << "tess " << tesselation << std::endl;
	ss << "s 0 0 0 1" << std::endl;

	std::string s = ss.str();
	std::string format = "nff";

	const aiScene* scene = importer.ReadFileFromMemory(s.data(), s.size(), ImportFlags, format.c_str());

	UINT32 numVertices = 0;
	UINT32 numIndices = 0;
	for (unsigned int i = 0; i < scene->mNumMeshes; i++)
	{
		numVertices += scene->mMeshes[i]->mNumVertices;
		numIndices += scene->mMeshes[i]->mNumFaces * 3;
	}

	mesh->m_Vertices.reserve(numVertices);
	mesh->m_Indices.reserve(numIndices);
	mesh->m_Materials.reserve(scene->mNumMaterials);

	for (unsigned int i = 0; i < scene->mNumMeshes; i++)
		mesh->InitSubMesh(i, scene->mMeshes[i], nullptr);
	mesh->InitMaterials(scene, "");

	return mesh;
}

Ref<Mesh> Mesh::CreateCube()
{
	Ref<Mesh> mesh = make_ref<Mesh>();
	Assimp::Importer importer;

	unsigned int ImportFlags =
		aiProcess_GenNormals |
		aiProcess_CalcTangentSpace |
		aiProcess_Triangulate |
		aiProcess_GenUVCoords |
		aiProcess_ConvertToLeftHanded;

	std::stringstream ss;

	ss << "hex 0 0 0 1" << std::endl;

	std::string s = ss.str();
	std::string format = "nff";

	const aiScene* scene = importer.ReadFileFromMemory(s.data(), s.size(), ImportFlags, format.c_str());

	UINT32 numVertices = 0;
	UINT32 numIndices = 0;
	for (unsigned int i = 0; i < scene->mNumMeshes; i++)
	{
		numVertices += scene->mMeshes[i]->mNumVertices;
		numIndices += scene->mMeshes[i]->mNumFaces * 3;
	}

	mesh->m_Vertices.reserve(numVertices);
	mesh->m_Indices.reserve(numIndices);
	mesh->m_Materials.reserve(scene->mNumMaterials);

	for (unsigned int i = 0; i < scene->mNumMeshes; i++)
		mesh->InitSubMesh(i, scene->mMeshes[i], nullptr);
	mesh->InitMaterials(scene, "");

	return mesh;
}