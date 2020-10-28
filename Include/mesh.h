#ifndef MESH_H
#define MESH_H

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <shader.h>

#include <string>
#include <vector>

namespace xre
{
	struct Vertex
	{
		// Position
		glm::vec3 position;

		// Normal
		glm::vec3 normal;

		// Texture Coordinates
		glm::vec2 tex_coords;

		// Tangent
		glm::vec3 tangent;

		// BitTangent
		glm::vec3 bit_tangent;
	};

	struct Texture
	{
		unsigned int id;
		std::string type;
		std::string path;
	};

	class Mesh
	{
	public:
		// Mesh Data
		std::vector<Vertex>			vertices;
		std::vector<unsigned int>	indices;
		std::vector<Texture>		textures;

		// Mesh Constructor
		Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices, const std::vector<Texture>& textures);

		void draw(const Shader& shader, const std::string model_name, const glm::mat4& model_matrix, const bool& is_dynamic);

	private:
		unsigned int VAO, VBO, EBO;
		bool setup_success;
		static std::vector<std::string> texture_types;

		void setupMesh();
	};
}
#endif