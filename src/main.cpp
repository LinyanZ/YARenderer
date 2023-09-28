#include <iostream>
#include <filesystem>
#include <string>

#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>
#include <assimp/Exporter.hpp>

#include <imgui.h>

int main(int argc, char const *argv[])
{
	std::string filename = "C:\\dev\\YARenderer\\resources\\city\\scene.gltf";
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

	const aiScene *scene = nullptr;
	scene = importer.ReadFile(filename, ImportFlags);

	std::cout << ((scene) ? "Yes" : "No") << std::endl;

	while (1)
		;

	return 0;
}
