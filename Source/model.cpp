#include <model.h>
#include <iostream>

#include <assimp/Importer.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <mesh.h>
#include <image_loader.h>
#include <shader.h>

#include <string>
#include <vector>
#include <chrono>

using namespace xre;

auto LOGGER = xre::LogModule::getLoggerInstance();

Model::Model(const std::string& file_path, const std::string& name, unsigned int post_process_options)
{
	auto start = std::chrono::high_resolution_clock::now;
	LOGGER->log(INFO, "Model", "Importing - " + file_path);

	model_name = name;
	setup_success = false;
	Assimp::Importer importer;

	scene = importer.ReadFile(file_path, post_process_options);

	if (!scene)
	{
		LOGGER->log(ERROR, "Model Import", "Failed to load model - " + file_path + " : Scene is empty!");
		return;
	}

	dynamic = false;
	model_matrix = glm::mat4(1.0);
	directory = file_path.substr(0, file_path.find_last_of('/'));
	processNode(scene->mRootNode);
}

void Model::draw(const Shader& model_shader,const std::string& model_name)
{
	for (unsigned i = 0; i < meshes.size(); i++)
	{
		if (meshes[i] != NULL)
		{
			meshes[i]->draw(model_shader, model_name, model_matrix, dynamic);
		}
	}
}

void Model::processNode(aiNode* node)
{
	for (unsigned int i = 0; i < node->mNumMeshes; i++)
	{
		meshes.push_back(extractMeshData(scene->mMeshes[node->mMeshes[i]]));
	}

	for (unsigned int i = 0; i < node->mNumChildren; i++)
	{
		processNode(node->mChildren[i]);
	}
}

Mesh* Model::extractMeshData(aiMesh* ai_mesh)
{
	std::vector<Vertex> vertices;
	std::vector<Texture> textures;
	std::vector<unsigned int> indices;

	// Vertex Data
	for (unsigned int i = 0; i < ai_mesh->mNumVertices; i++)
	{
		try
		{
			Vertex v;

			// Vertex Positions
			if (ai_mesh->HasPositions())
			{
				v.position.x = ai_mesh->mVertices[i].x;
				v.position.y = ai_mesh->mVertices[i].y;
				v.position.z = ai_mesh->mVertices[i].z;
			}
			else
			{
				VertexDataException e;
				e.exception_message = "No vertex position data";
				throw e;
			}

			// Vertex Normals
			if (ai_mesh->HasNormals())
			{
				v.normal.x = ai_mesh->mNormals[i].x;
				v.normal.y = ai_mesh->mNormals[i].y;
				v.normal.z = ai_mesh->mNormals[i].z;
			}
			else
			{
				VertexDataException e;
				e.exception_message = "No vertex normal data";
				throw e;
			}

			// Texture Coordinates
			if (ai_mesh->HasTextureCoords(0))
			{
				v.tex_coords.x = ai_mesh->mTextureCoords[0][i].x;
				v.tex_coords.y = ai_mesh->mTextureCoords[0][i].y;
			}
			else
			{
				LOGGER->log(WARN, "Mesh Data Extraction", model_name + " : " + "No texture coordinates found! Texture coordinates will be set to (0 , 0)");
				v.tex_coords.x = 0.0f;
				v.tex_coords.y = 0.0f;
			}

			// Tangents
			if (ai_mesh->HasTangentsAndBitangents())
			{
				v.tangent.x = ai_mesh->mTangents[i].x;
				v.tangent.y = ai_mesh->mTangents[i].y;
				v.tangent.z = ai_mesh->mTangents[i].z;

				v.bit_tangent.x = ai_mesh->mBitangents[i].x;
				v.bit_tangent.y = ai_mesh->mBitangents[i].y;
				v.bit_tangent.z = ai_mesh->mBitangents[i].z;
			}
			else
			{
				LOGGER->log(WARN, "Mesh Data Extraction", model_name + " : " + "Tangents data not found! Tangents and bit_tangents will be set to (0, 0, 0)");
				v.tangent.x = v.tangent.y = v.tangent.z = 0;
				v.bit_tangent.x = v.bit_tangent.y = v.bit_tangent.z = 0;
			}

			vertices.push_back(v);
		}
		catch (const VertexDataException& e)
		{
			LOGGER->log(ERROR, "Mesh Data Extraction", model_name + " : " + e.exception_message);
			setup_success = false;
			return NULL;
		}
		catch (const std::exception& e)
		{
			LOGGER->log(ERROR, "Mesh Data Extraction", model_name + " : " + e.what());
			setup_success = false;
			return NULL;
		}
	}

	// Indices
	for (unsigned int i = 0; i < ai_mesh->mNumFaces; i++)
	{
		aiFace face = ai_mesh->mFaces[i];
		for (unsigned int j = 0; j < face.mNumIndices; j++)
		{
			indices.push_back(face.mIndices[j]);
		}
	}

	aiMaterial* material = scene->mMaterials[ai_mesh->mMaterialIndex];

	LOGGER->log(INFO, "LOAD TEXTURE", "Loading diffuse texture.");
	Texture diffuse_texture = loadTextureFromPath(material, aiTextureType_DIFFUSE, "texture_diffuse");
	textures.push_back(diffuse_texture);

	LOGGER->log(INFO, "LOAD TEXTURE", "Loading specular texture.");
	Texture specular_texture = loadTextureFromPath(material, aiTextureType_SPECULAR, "texture_specular");
	textures.push_back(specular_texture);

	LOGGER->log(INFO, "LOAD TEXTURE", "Loading normal texture.");
	Texture normal_texture = loadTextureFromPath(material, aiTextureType_NORMALS, "texture_normal");
	textures.push_back(normal_texture);

	return new Mesh(vertices, indices, textures);
}

Texture Model::loadTextureFromPath(aiMaterial* material, aiTextureType texture_type, const std::string& texture_type_name)
{
	Texture t;

	aiString str;
	material->GetTexture(texture_type, 0, &str);

	bool skip = false;
	for (unsigned int i = 0; i < loaded_textures.size(); i++)
	{
		if (std::strcmp(loaded_textures[i].path.data(), str.C_Str()) == 0)
		{
			t = loaded_textures[i];
			skip = true;
			break;
		}
	}
	if (!skip)
	{
		t.id = GetTexture(str.C_Str(), texture_type == aiTextureType_DIFFUSE);
		t.type = texture_type_name;
		t.path = str.C_Str();
		loaded_textures.push_back(t);
	}

	return t;
}

unsigned int Model::GetTexture(const std::string& texture_path, bool gamma)
{
	std::string file_path = directory + '/' + texture_path;

	LOGGER->log(DEBUG, "LOAD TEXTURE", texture_path + " : " + file_path);

	unsigned int texture_id;
	glGenTextures(1, &texture_id);

	int texture_width, texture_height, num_channels;
	unsigned char* data = stbi_load(file_path.c_str(), &texture_width, &texture_height, &num_channels, 0);

	if (data)
	{
		unsigned int format = GL_RGB;
		if (num_channels == 1)
			format = GL_RED;
		else if (num_channels == 3)
			format = GL_RGB;
		else if(num_channels == 4)
			format = GL_RGBA;

		unsigned int internal_format = gamma ? GL_SRGB_ALPHA : format;

		glBindTexture(GL_TEXTURE_2D, texture_id);
		glTexImage2D(GL_TEXTURE_2D, 0,internal_format, texture_width, texture_height, 0, format, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		stbi_image_free(data);
	}		
	else
	{
		LOGGER->log(xre::ERROR, "LOAD TEXTURE", "Failed to create texture : " + std::string(file_path) + " : " + stbi_failure_reason());
		stbi_image_free(data);
	}

	return texture_id;
}

void Model::translate(glm::vec3 translation)
{
	model_matrix = glm::translate(model_matrix, translation);
}

void Model::rotate(float amount, glm::vec3 axes)
{
	model_matrix = glm::rotate(model_matrix, amount, axes);
}

void Model::scale(glm::vec3 axes)
{
	model_matrix = glm::scale(model_matrix, axes);
}