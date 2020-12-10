#include <mesh.h>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <shader.h>
#include <logger.h>
#include <renderer.h>

#include <string>
#include <vector>

static auto LOGGER = xre::LogModule::getLoggerInstance();

using namespace xre;

std::vector<std::string> Mesh::texture_types{ "texture_diffuse", "texture_specular", "texture_normal", "shadow_depth_map_directional", "shadow_depth_map_cubemap" };

Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices, const std::vector<Texture>& textures, BoundingVolume aabb)
{
	setup_success = false;
	try
	{
		if (vertices.size() > 0 && indices.size() > 0)
		{
			this->vertices = vertices;
			this->indices = indices;
		}
		else
			throw;

		if (textures.size() > 0)
		{
			this->textures = textures;
		}
		else
		{
			LOGGER->log(WARN, "Mesh - Constructor", "Empty Textures Vector.");
		}


		this->aabb = aabb;

		setupMesh();
	}
	catch (...)
	{
		LOGGER->log(ERROR, "Mesh - Constructor", "Empty Vertices/Indices Vector.");
		return;
	}
}


void Mesh::draw(const Shader& shader, const std::string model_name, const glm::mat4& model_matrix, const bool& is_dynamic)
{
	//if (!setup_success)
	//	return;

	RenderSystem::renderer()->draw(VAO, indices.size(), shader, model_matrix, &textures, &texture_types, model_name, is_dynamic, &setup_success, aabb);

}

void Mesh::setupMesh()
{
	try
	{
		// create buffers/arrays
		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO);
		glGenBuffers(1, &EBO);

		glBindVertexArray(VAO);
		// load data into vertex buffers
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		// A great thing about structs is that their memory layout is sequential for all its items.
		// The effect is that we can simply pass a pointer to the struct and it translates perfectly to a glm::vec3/2 array which
		// again translates to 3/2 floats which translates to a byte array.
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

		// set the vertex attribute pointers
		// vertex Positions
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
		// vertex normals
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
		// vertex texture coords
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, tex_coords));
		// vertex tangent
		glEnableVertexAttribArray(3);
		glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, tangent));
		// vertex bitangent
		glEnableVertexAttribArray(4);
		glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, bit_tangent));

		glBindVertexArray(0);

		setup_success = true;
	}
	catch (...)
	{
		LOGGER->log(ERROR, "Mesh - setupMesh()", "Failed to setup mesh!");
	}
}