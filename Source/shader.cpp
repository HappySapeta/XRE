#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>


#include <shader.h>
#include <logger.h>

#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#include <tuple>

static auto LOGGER = xre::LogModule::getLoggerInstance();

using namespace xre;

Shader::Shader(const char* vertex_shader_path, const char* fragment_shader_path, const char* geometry_shader_path)
{
	shader_program_id = glCreateProgram();

	std::string vertex_shader_code, fragment_shader_code;
	std::ifstream vertex_shader_file, fragment_shader_file;

	vertex_shader_file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
	fragment_shader_file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
	try
	{
		LOGGER->log(xre::INFO, "SHADER : VERTEX", "Reading - " + std::string(vertex_shader_path));
		vertex_shader_file.open(vertex_shader_path);
		std::stringstream vertex_shader_stream;

		vertex_shader_stream << vertex_shader_file.rdbuf();
		vertex_shader_file.close();
		vertex_shader_code = vertex_shader_stream.str();

		LOGGER->log(xre::INFO, "SHADER : FRAGMENT", "Reading - " + std::string(fragment_shader_path));
		fragment_shader_file.open(fragment_shader_path);
		std::stringstream fragment_shader_stream;

		fragment_shader_stream << fragment_shader_file.rdbuf();
		fragment_shader_file.close();
		fragment_shader_code = fragment_shader_stream.str();

		//LOGGER->log(xre::INFO, type, "Shader read successful!");
	}
	catch (std::ifstream::failure& e)
	{
		LOGGER->log(xre::ERROR, "SHADER : VERTEX/FRAGMENT", "Failed to read shader file : " + std::string(e.what()));
		return;
	}

	const char* c_vertex_shader_code = vertex_shader_code.c_str();
	const char* c_fragment_shader_code = fragment_shader_code.c_str();

	unsigned int vertex_shader, fragment_shader, geometry_shader = -1;

	vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex_shader, 1, &c_vertex_shader_code, NULL);
	glCompileShader(vertex_shader);
	checkCompileErrors(vertex_shader, "SHADER");

	fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment_shader, 1, &c_fragment_shader_code, NULL);
	glCompileShader(fragment_shader);
	checkCompileErrors(fragment_shader, "SHADER");

	glAttachShader(shader_program_id, vertex_shader);
	glAttachShader(shader_program_id, fragment_shader);

	// Geometry shader
	// ---------------------------------------------------
	if (geometry_shader_path != NULL)
	{
		LOGGER->log(xre::INFO, "SHADER : GEOMETRY", "Reading - " + std::string(geometry_shader_path));
		std::string geometry_shader_code;
		std::ifstream geometry_shader_file;

		geometry_shader_file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
		try
		{
			geometry_shader_file.open(geometry_shader_path);
			std::stringstream geometry_shader_stream;

			geometry_shader_stream << geometry_shader_file.rdbuf();
			geometry_shader_file.close();
			geometry_shader_code = geometry_shader_stream.str();
		}
		catch (std::ifstream::failure& e)
		{
			LOGGER->log(xre::ERROR, "SHADER : GEOMETRY", "Failed to read shader file : " + std::string(e.what()));
		}

		const char* c_geometry_shader_code = geometry_shader_code.c_str();

		geometry_shader = glCreateShader(GL_GEOMETRY_SHADER);
		glShaderSource(geometry_shader, 1, &c_geometry_shader_code, NULL);
		glCompileShader(geometry_shader);
		checkCompileErrors(geometry_shader, "SHADER");
		glAttachShader(shader_program_id, geometry_shader);
	}
	// Geometry shader
	// ---------------------------------------------------

	glLinkProgram(shader_program_id);

	checkCompileErrors(shader_program_id, "PROGRAM");

	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);

	if (geometry_shader != -1)
		glDeleteShader(geometry_shader);
}

void Shader::setBool(std::string uniform_name, bool value) const { glUniform1i(glGetUniformLocation(shader_program_id, uniform_name.c_str()), value); }
void Shader::setInt(std::string uniform_name, int value) const { glUniform1i(glGetUniformLocation(shader_program_id, uniform_name.c_str()), value); }
void Shader::setFloat(std::string uniform_name, float value)const { glUniform1f(glGetUniformLocation(shader_program_id, uniform_name.c_str()), value); }
void Shader::setMat4(std::string uniform_name, glm::mat4 value) const { glUniformMatrix4fv(glGetUniformLocation(shader_program_id, uniform_name.c_str()), 1, GL_FALSE, glm::value_ptr(value)); }
void Shader::setVec3(std::string uniform_name, glm::vec3 value) const { glUniform3fv(glGetUniformLocation(shader_program_id, uniform_name.c_str()), 1, glm::value_ptr(value)); }
void Shader::setVec4(std::string uniform_name, glm::vec4 value) const { glUniform4fv(glGetUniformLocation(shader_program_id, uniform_name.c_str()), 1, glm::value_ptr(value)); }

void Shader::checkCompileErrors(unsigned int& shader, const std::string& type)
{
	int success;
	char infoLog[1024];

	if (type != "PROGRAM")
	{
		glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
		if (!success)
		{
			glGetShaderInfoLog(shader, 1024, NULL, infoLog);
			LOGGER->log(xre::ERROR, "SHADER COMPILATION : " + type, "Failed to compile shader : " + std::string(infoLog));
		}
	}
	else
	{
		glGetProgramiv(shader, GL_LINK_STATUS, &success);
		if (!success)
		{
			glGetProgramInfoLog(shader, 1024, NULL, infoLog);
			LOGGER->log(xre::ERROR, "PROGRAM LINKING", "Failed link shader program : " + std::string(infoLog));
		}
	}
}

void Shader::use() const
{
	glUseProgram(shader_program_id);
}