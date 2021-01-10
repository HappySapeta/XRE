#ifndef LIGHTING_PROBES_H
#define LIGHTING_PROBES_H

#include <glm/glm.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <shader.h>
#include <renderer.h>

#include <vector>

namespace xre
{
	class ProbeRenderer
	{
	private:

		struct LightProbe
		{
			glm::vec3 position;

			unsigned int render_texture;
			//unsigned int diffuse_irradiance_cubemap;
		};

		struct zone
		{
			unsigned int num_light_probes;
			LightProbe* light_probes;

			zone* left_zone;
			zone* right_zone;

		};

		unsigned int irradiance_map_resolution;
		unsigned int reflection_map_resolution;
		unsigned int rendering_resolution;
		
		std::vector<LightProbe> light_probes;

		ProbeRenderer();

#pragma region Probe Rendering Data

		unsigned int probeFBO, renderFBO;
		unsigned int probeRBO, renderRBO;
		bool init_success;

		unsigned int cubeVAO;
		unsigned int cubeVBO;

		Shader* diffuseIrradianceShader;
		Shader* specularIrradianceShader;
		Shader* renderingShader;

		glm::mat4 captureProjection;
		glm::mat4 captureViews[6] =
		{
			glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
			glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
			glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
			glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
			glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
			glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
		};
		glm::vec3 render_views_eye_center[6] =
		{
			glm::vec3(1.0f,  0.0f,  0.0f),
			glm::vec3(-1.0f,  0.0f,  0.0f),
			glm::vec3(0.0f,  1.0f,  0.0f),
			glm::vec3(0.0f, -1.0f,  0.0f),
			glm::vec3(0.0f,  0.0f,  1.0f),
			glm::vec3(0.0f,  0.0f, -1.0f)
		};

		glm::vec3 render_views_eye_up[6] =
		{
			glm::vec3(0.0f, -1.0f,  0.0f),
			glm::vec3(0.0f, -1.0f,  0.0f),
			glm::vec3(0.0f,  0.0f,  1.0f),
			glm::vec3(0.0f,  0.0f, -1.0f),
			glm::vec3(0.0f, -1.0f,  0.0f),
			glm::vec3(0.0f, -1.0f,  0.0f)
		};

#pragma endregion

	public:

		unsigned int light_probe_diffuse_irradiance_cubemap_array;
		unsigned int light_probe_specular_irradiance_cubemap_array;

		ProbeRenderer(unsigned int irradiance_map_resolution, unsigned int relection_map_resolution, unsigned int rendering_resolution);

		void RenderProbes(
			std::vector<model_information>* draw_queue, 
			std::vector<PointLight*>* point_lights, 
			DirectionalLight* directional_light, 
			glm::mat4* directional_light_space_matrix, 
			unsigned int* point_shadow_depth_storage,
			unsigned int& directional_shadow_depth_storage);
		void GenerateLightProbes(glm::vec3 span, glm::vec3 offset, glm::vec3 probe_density, bool debug_probes = false);
		void SetShaderAttributes(Shader* main_lighting_shader);
	};
}

#endif