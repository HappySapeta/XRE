#include <lights.h>

#include <iostream>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <shader.h>
#include <renderer.h>
#include <logger.h>

using namespace xre;

static auto LOGGER = LogModule::getLoggerInstance();

#pragma region Light
Light::Light()
	: m_position(glm::vec3(0.0f)), m_color(glm::vec3(0.0f)), m_direction(glm::vec3(0.0f)), m_intensityMultiplier(0) {}

void Light::Translate(const glm::vec3& vector)
{
	std::cout << "Cannot move lights of this type." << std::endl;
}

void Light::SetDirection(const glm::vec3& direction)
{
	std::cout << "Cannot change direction of lights of this type." << std::endl;
}

void Light::SetShaderAttrib(const std::string& lightuniform, const Shader& shader)
{}
#pragma endregion

#pragma region DirectionalLight

DirectionalLight::DirectionalLight(const glm::vec3 position, const glm::vec3& color, const float& im, std::string light_name)
{
	m_position = position;
	m_color = color;
	m_intensityMultiplier = im;
	m_name = light_name;
	m_direction = glm::normalize(glm::vec3(0.0f) - position);

	RenderSystem::renderer()->addToRenderSystem(this);
}

DirectionalLight::DirectionalLight()
{
	m_direction = glm::vec3(0.0f);
	m_position = glm::vec3(0.0f);
	m_intensityMultiplier = 1.0f;
	m_color = glm::vec3(0.0f);
}

void DirectionalLight::SetDirection(const glm::vec3& direction)
{
	m_direction = direction;
}

void  DirectionalLight::SetShaderAttrib(const std::string& lightuniform, const Shader& shader)
{
	shader.setVec3(lightuniform + ".position", m_position);
	shader.setVec3(lightuniform + ".direction", m_direction);
	shader.setVec3(lightuniform + ".color", m_color * m_intensityMultiplier);
}
#pragma endregion

#pragma region PointLight
PointLight::PointLight()
	: m_constantFalloff(1), m_linearFalloff(1), m_quadraticFalloff(1) {}

PointLight::PointLight(const glm::vec3& position, const glm::vec3& color, const float& kc, const float& kl, const float& kq, const float& im, std::string light_name)
	: m_constantFalloff(kc), m_linearFalloff(kl), m_quadraticFalloff(kq)
{
	m_position = position;
	m_color = color;
	m_intensityMultiplier = im;
	m_name = light_name;
	RenderSystem::renderer()->addToRenderSystem(this);
}

void PointLight::Translate(const glm::vec3& vector)
{
	m_position += vector;
}

void PointLight::SetShaderAttrib(const std::string& lightuniform, const Shader& shader)
{
	shader.setVec3(lightuniform + ".position", m_position);
	shader.setVec3(lightuniform + ".color", m_color * m_intensityMultiplier);
	shader.setFloat(lightuniform + ".kc", m_constantFalloff);
	shader.setFloat(lightuniform + ".kl", m_linearFalloff);
	shader.setFloat(lightuniform + ".kq", m_quadraticFalloff);
}
#pragma endregion

#pragma region SpotLight
SpotLight::SpotLight(const glm::vec3& position, const glm::vec3& direction, const glm::vec3& color, const float& innerCuttOff, const float& outerCutOff, const float& kc, const float& kl, const float& kq, const float& intensity, std::string light_name)
	: m_innerCutOff(innerCuttOff), m_outerCutOff(outerCutOff)
{
	Light::m_position = position;
	Light::m_direction = direction;
	Light::m_color = color;
	Light::m_intensityMultiplier = intensity;
	m_constantFalloff = kc;
	m_linearFalloff = kl;
	m_quadraticFalloff = kq;
	m_name = light_name;
	RenderSystem::renderer()->addToRenderSystem(this);
}

void SpotLight::Translate(const glm::vec3& vector)
{
	Light::m_position += vector;
}

void SpotLight::SetDirection(const glm::vec3& direction)
{
	Light::m_direction = direction;
}

void SpotLight::SetPosition(const glm::vec3& position)
{
	Light::m_position = position;
}

void SpotLight::SetShaderAttrib(const std::string& lightuniform, const Shader& shader)
{
	shader.setVec3(lightuniform + ".position", Light::m_position);
	shader.setVec3(lightuniform + ".direction", Light::m_direction);
	shader.setVec3(lightuniform + ".color", m_color * m_intensityMultiplier);
	shader.setFloat(lightuniform + ".kc", m_constantFalloff);
	shader.setFloat(lightuniform + ".kl", m_linearFalloff);
	shader.setFloat(lightuniform + ".kq", m_quadraticFalloff);
	shader.setFloat(lightuniform + ".innercutoff", m_innerCutOff);
	shader.setFloat(lightuniform + ".outercutoff", m_outerCutOff);
}
#pragma endregion