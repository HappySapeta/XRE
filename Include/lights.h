#ifndef LIGHTS_H
#define LIGHTS_H

#include <iostream>
#include <glm/glm.hpp>
#include <shader.h>

namespace xre
{
	class Light
	{
	public:
		glm::vec3 m_color;
		glm::vec3 m_position;
		glm::vec3 m_direction;
		float m_intensityMultiplier;
		std::string m_name;

		Light();
		
		virtual void Translate(const glm::vec3& vector);
		virtual void SetDirection(const glm::vec3& direction);
		virtual void SetShaderAttrib(const std::string& lightuniform, const Shader& shader);
	};

	class DirectionalLight : public Light
	{
	public:
		DirectionalLight(const glm::vec3 position, const glm::vec3& color, const float& im, std::string light_name);
		DirectionalLight();

		void SetDirection(const glm::vec3& direction) override;
		void SetShaderAttrib(const std::string& lightuniform, const Shader& shader) override;
	};

	class PointLight : public Light
	{
	public:
		float m_constantFalloff;
		float m_linearFalloff;
		float m_quadraticFalloff;

		PointLight();
		PointLight(const glm::vec3& position, const glm::vec3& color, const float& kc, const float& kl, const float& kq, const float& im, std::string light_name);

		void Translate(const glm::vec3& vector) override;
		void SetShaderAttrib(const std::string& lightuniform, const Shader& shader) override;
	};


	class SpotLight : public PointLight
	{
	public:
		float m_innerCutOff;
		float m_outerCutOff;

		SpotLight(const glm::vec3& position, const glm::vec3& direction, const glm::vec3& color, const float& innerCuttOff, const float& outerCutOff, const float& kc, const float& kl, const float& kq, const float& intensity, std::string light_name);

		void Translate(const glm::vec3& vector) override;
		void SetDirection(const glm::vec3& direction) override;
		void SetPosition(const glm::vec3& position);
		void SetShaderAttrib(const std::string& lightuniform, const Shader& shader) override;
	};
}
#endif