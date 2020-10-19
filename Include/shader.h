#ifndef SHADER_H
#define SHADER_H

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <logger.h>

#include <variant>
#include <string>
#include <unordered_map>

namespace xre
{
	class Shader
	{
	public:
		unsigned int shader_program_id;

		Shader(const char* vertex_shader_path, const char* fragment_shader_path, const char* geometry_shader_path = NULL);

		// Set Uniform Functions
		void setBool(std::string uniform_name, bool value) const;
		void setInt(std::string uniform_name, int value) const;
		void setFloat(std::string uniform_name, float value) const;
		void setMat4(std::string uniform_name, glm::mat4 value) const;
		void setVec3(std::string uniform_name, glm::vec3 value) const;
		void setVec4(std::string uniform_name, glm::vec4 value) const;

		// Activate the shader
		void use() const;

	private:

		//  Utility function for checking compliation / linking errors
		void checkCompileErrors(unsigned int& shader, const std::string& type);

		// Data Type to Uniform function mapping
		typedef void(xre::Shader::* functionPointer)();
		std::unordered_map<std::string, functionPointer> uniform_function_map;
	};
}
#endif