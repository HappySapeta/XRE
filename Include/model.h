#ifndef MODEL_H
#define MODEL_H

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <mesh.h>
#include <shader.h>

#include <string>
#include <vector>
#include <exception>

namespace xre
{
	struct VertexDataException : public std::exception {
		std::string exception_message;
	};

	class Model
	{
	public: 

		Model(const std::string& file_path,const std::string& name,unsigned int = aiProcess_Triangulate);
		void draw(const Shader& model_shader,const std::string& model_name);
		
		void translate(glm::vec3 translation);
		void rotate(float amount, glm::vec3 axes);
		void scale(glm::vec3 axes);

		bool dynamic;
		glm::mat4 model_matrix;

	private:
		const aiScene* scene;
		std::string model_name;
		std::string directory;
		std::vector<Texture> loaded_textures;

		bool setup_success;

		void processNode(aiNode* node);
		Texture loadTextureFromPath(aiMaterial* material, aiTextureType texture_type, const std::string& texture_type_name);
		Mesh* extractMeshData(aiMesh* ai_mesh);
		unsigned int GetTexture(const std::string& texture_path, bool gamma = false);

		std::vector<Mesh*> meshes;
	};
}

#endif