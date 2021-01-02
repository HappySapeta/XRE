#include <LightingProbes.h>

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <sstream>

#include <renderer.h>
#include <logger.h>


using namespace xre;

const char* GLErrorToString(GLenum err);
static LogModule* LOGGER = LogModule::getLoggerInstance();

xre::ProbeRenderer::ProbeRenderer(unsigned int lightmap_resolution, unsigned int rendering_resolution)
{
	init_success = true;

	ProbeRenderer::lightmap_resolution = lightmap_resolution;
	ProbeRenderer::rendering_resolution = rendering_resolution;

	glGenFramebuffers(1, &probeFBO);
	glGenRenderbuffers(1, &probeRBO);

	glBindFramebuffer(GL_FRAMEBUFFER, probeFBO);
	glBindRenderbuffer(GL_RENDERBUFFER, probeRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, lightmap_resolution, lightmap_resolution);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, probeRBO);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		LOGGER->log(ERROR, "xre::ProbeRenderer::ProbeRenderer", "probeFBO is incomplete!");
		init_success = false;
	}

	glGenFramebuffers(1, &renderFBO);
	glGenRenderbuffers(1, &renderRBO);

	glBindFramebuffer(GL_FRAMEBUFFER, renderFBO);
	glBindRenderbuffer(GL_RENDERBUFFER, renderRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, rendering_resolution, rendering_resolution);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, renderRBO);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		LOGGER->log(ERROR, "xre::ProbeRenderer::ProbeRenderer", "renderFBO is incomplete!");
		init_success = false;
	}

	irradianceShader = new Shader(
		"./Source/Resources/Shaders/IBL/lightmap_vertex_shader.vert",
		"./Source/Resources/Shaders/IBL/irradiance_fragment_shader.frag");

	lightingShader = new Shader(
		"./Source/Resources/Shaders/IBL/forward_bphong_shadowless_vertex_shader.vert",
		"./Source/Resources/Shaders/IBL/forward_bphong_shadowless_fragment_shader.frag"
	);

	render_views_eye_center[0] = glm::vec3(1.0f, 0.0f, 0.0f);
	render_views_eye_center[1] = glm::vec3(-1.0f, 0.0f, 0.0f);
	render_views_eye_center[2] = glm::vec3(0.0f, 1.0f, 0.0f);
	render_views_eye_center[3] = glm::vec3(0.0f, -1.0f, 0.0f);
	render_views_eye_center[4] = glm::vec3(0.0f, 0.0f, 1.0f);
	render_views_eye_center[5] = glm::vec3(0.0f, 0.0f, -1.0f);

	render_views_eye_up[0] = glm::vec3(0.0f, -1.0f, 0.0f);
	render_views_eye_up[1] = glm::vec3(0.0f, -1.0f, 0.0f);
	render_views_eye_up[2] = glm::vec3(0.0f, 0.0f, 1.0f);
	render_views_eye_up[3] = glm::vec3(0.0f, 0.0f, -1.0f);
	render_views_eye_up[4] = glm::vec3(0.0f, -1.0f, 0.0f);
	render_views_eye_up[5] = glm::vec3(0.0f, -1.0f, 0.0f);

	convolution_views[0] = glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f));
	convolution_views[1] = glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f));
	convolution_views[2] = glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	convolution_views[3] = glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f));
	convolution_views[4] = glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f));
	convolution_views[5] = glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f));


	float vertices[] = {
		// back face
		-1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
		 1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
		 1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, // bottom-right         
		 1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
		-1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
		-1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, // top-left
		// front face
		-1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
		 1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f, // bottom-right
		 1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
		 1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
		-1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, // top-left
		-1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
		// left face
		-1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
		-1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-left
		-1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
		-1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
		-1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-right
		-1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
		// right face
		 1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
		 1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
		 1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-right         
		 1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
		 1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
		 1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-left     
		// bottom face
		-1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
		 1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f, // top-left
		 1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
		 1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
		-1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f, // bottom-right
		-1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
		// top face
		-1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
		 1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
		 1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f, // top-right     
		 1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
		-1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
		-1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f  // bottom-left        
	};

	glGenVertexArrays(1, &cubeVAO);
	glGenBuffers(1, &cubeVBO);
	// fill buffer
	glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	// link vertex attributes
	glBindVertexArray(cubeVAO);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

void xre::ProbeRenderer::GenerateLightProbes(
	glm::vec3 span, glm::vec3 offset, glm::vec3 probe_density, bool debug_probes)
{
	if (!init_success)
	{
		return;
	}

	float step_x = 1 / probe_density.x;
	float step_y = 1 / probe_density.y;
	float step_z = 1 / probe_density.z;

	float x_pos = -span.x / 2;
	float y_pos = -span.y / 2;
	float z_pos = span.z / 2;

	unsigned int num_probes = 0;

	while (y_pos <= span.y)
	{
		while (z_pos >= -span.z)
		{
			while (x_pos <= span.x)
			{
				glm::vec3 pos = glm::vec3(x_pos, y_pos, z_pos);

				LightProbe lp;
				lp.position = pos - offset;

				glGenTextures(1, &lp.render_texture);
				glBindTexture(GL_TEXTURE_CUBE_MAP, lp.render_texture);

				for (unsigned int i = 0; i < 6; i++)
				{
					glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, rendering_resolution, rendering_resolution, 0, GL_RGB, GL_FLOAT, NULL);
				}
				glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

				light_probes.push_back(lp);

				num_probes++;

				x_pos += step_x;
			}
			x_pos = -span.x / 2;
			z_pos -= step_z;
		}
		z_pos = span.z / 2;
		y_pos += step_y;
	}

	glGenTextures(1, &light_probe_cubemap_array);
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, light_probe_cubemap_array);

	glTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_RGB, lightmap_resolution, lightmap_resolution, num_probes * 6, 0, GL_RGB, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	std::stringstream ss;
	ss << num_probes << " light probes were generated.";

	LOGGER->log(INFO, "xre::ProbeRenderer::GenerateLightProbes", ss.str());
}

void xre::ProbeRenderer::RenderProbes(
	std::vector<model_information>* draw_queue,
	std::vector<PointLight*>* point_lights,
	DirectionalLight* directional_light,
	glm::mat4* directional_light_space_matrix,
	unsigned int* point_shadow_depth_storage,
	unsigned int& directional_shadow_depth_storage)
{

#pragma region LightPass

	glBindFramebuffer(GL_FRAMEBUFFER, renderFBO);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	glViewport(0, 0, rendering_resolution, rendering_resolution);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	if (directional_light != NULL)
	{
		glClearColor(1.0, 1.0, 1.0, 1.0);
	}
	else
	{
		glClearColor(0.0, 0.0, 0.0, 1.0);
	}

	glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.0f);

	lightingShader->use();

	lightingShader->setInt("N_POINT", point_lights->size());
	lightingShader->setBool("directional_lighting_enabled", directional_light != NULL);
	lightingShader->setMat4("projection", projection);
	lightingShader->setFloat("shininess", 32);
	lightingShader->setFloat("near", 0.1f);
	lightingShader->setFloat("far", 100.0f);

	std::stringstream ss;

	for (unsigned int p = 0; p < light_probes.size(); p++)  //optimize state changes.
	{
		lightingShader->setVec3("camera_pos", light_probes[p].position);

		for (unsigned int s = 0; s < 6; s++)
		{
			glm::mat4 view = glm::lookAt(light_probes[p].position, light_probes[p].position + render_views_eye_center[s], render_views_eye_up[s]);

			lightingShader->setMat4("view", view);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + s, light_probes[p].render_texture, 0);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			for (unsigned int d = 0; d < draw_queue->size(); d++)
			{
				lightingShader->setMat4("model", *draw_queue->at(d).object_model_matrix);

				unsigned int j;
				for (j = 0; j < draw_queue->at(d).object_textures->size(); j++)
				{
					glActiveTexture(GL_TEXTURE0 + j);
					lightingShader->setInt(draw_queue->at(d).object_textures->at(j).type, j);
					glBindTexture(GL_TEXTURE_2D, draw_queue->at(d).object_textures->at(j).id);
				}

				if (point_lights->size() > 0)
				{
					for (unsigned int k = j; k < XRE_MAX_POINT_SHADOW_MAPS + j; k++)
					{
						ss.str("");
						ss << "[" << k - j << "]";
						glActiveTexture(GL_TEXTURE0 + k);
						lightingShader->setInt("point_shadow_depth_map" + ss.str(), k);
						glBindTexture(GL_TEXTURE_CUBE_MAP, point_shadow_depth_storage[k - j]);
					}
				}

				j += XRE_MAX_POINT_SHADOW_MAPS;

				if (directional_light != NULL)
				{
					lightingShader->setMat4("directional_light_space_matrix", *directional_light_space_matrix);

					glActiveTexture(GL_TEXTURE0 + j);
					lightingShader->setInt("directional_shadow_depth_map", j);
					glBindTexture(GL_TEXTURE_2D, directional_shadow_depth_storage);
				}

				for (unsigned int l = 0; l < point_lights->size(); l++)
				{
					point_lights->at(l)->SetShaderAttrib(point_lights->at(l)->m_name, *lightingShader);
				}

				if (directional_light)
				{
					directional_light->SetShaderAttrib(directional_light->m_name, *lightingShader);
				}

				glBindVertexArray(draw_queue->at(d).object_VAO);
				glDrawElements(GL_TRIANGLES, draw_queue->at(d).indices_size, GL_UNSIGNED_INT, 0);
				glBindVertexArray(0);
			}
		}
	}

#pragma endregion

#pragma region Convolution Pass

	irradianceShader->use();
	glBindFramebuffer(GL_FRAMEBUFFER, probeFBO);
	glViewport(0, 0, lightmap_resolution, lightmap_resolution);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);

	irradianceShader->setMat4("projection", projection);

	for (unsigned int p = 0; p < light_probes.size(); p++)
	{
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_CUBE_MAP, light_probes[p].render_texture);
		irradianceShader->setInt("envCubemap", 0);

		for (unsigned int s = 0; s < 6; s++)
		{
			irradianceShader->setMat4("view", convolution_views[s]);
			glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, light_probe_cubemap_array, 0, p * 6 + s);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			glBindVertexArray(cubeVAO);
			glDrawArrays(GL_TRIANGLES, 0, 36);
			glBindVertexArray(0);
		}

		glDeleteTextures(1, &light_probes[p].render_texture);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDisable(GL_DEPTH_TEST);

#pragma endregion
}

void xre::ProbeRenderer::SetShaderAttributes(Shader* main_lighting_shader)
{
	main_lighting_shader->use();
	std::stringstream ss;

	for (unsigned int i = 0; i < light_probes.size(); i++)
	{
		ss.str("");
		ss << "light_probes" << '[' << i << ']' << ".position";
		main_lighting_shader->setVec3(ss.str(), light_probes[i].position);
	}
}