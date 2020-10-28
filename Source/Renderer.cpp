#include <renderer.h>

#include <iostream>
#include <string>
#include <sstream>

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <GLFW/glfw3.h>

#include <logger.h>
#include <shader.h>
#include <lights.h>
#include <mesh.h>
#include <xre_configuration.h>

using namespace xre;

const char* GLErrorToString(GLenum err);

static LogModule* LOGGER = LogModule::getLoggerInstance();

RenderSystem* RenderSystem::instance = NULL;

RenderSystem* RenderSystem::renderer()
{
	if (instance == NULL)
	{
		LOGGER->log(ERROR, "Render System : renderer", "No renderer instance available. Create one first!");
	}
	return instance;
}

// SHOULD BE CALLED ONLY ONCE, BEFORE ENTERING RENDER LOOP.
RenderSystem* RenderSystem::renderer(unsigned int screen_width, unsigned int screen_height, const glm::vec4& background_color, float lights_near_plane_p, float lights_far_plane_p, int shadow_map_width_p, int shadow_map_height_p)
{
	if (instance == NULL)
	{
		instance = new RenderSystem(screen_width, screen_height, background_color, lights_near_plane_p, lights_far_plane_p, shadow_map_width_p, shadow_map_height_p);
	}
	return instance;
}

RenderSystem::RenderSystem(unsigned int screen_width, unsigned int screen_height, const glm::vec4& background_color, float lights_near_plane_p, float lights_far_plane_p, int shadow_map_width_p, int shadow_map_height_p)
	:framebuffer_width(screen_width), framebuffer_height(screen_height), bg_color(background_color), light_near_plane(lights_near_plane_p), light_far_plane(lights_far_plane_p), shadow_map_width(shadow_map_height_p), shadow_map_height(shadow_map_height_p)
{
	createFramebuffers();
	createShadowMapFramebuffer();
	createQuad();
	quadShader = new Shader(
		"D:/Github/XperimentalRenderingEngine/XRE/Source/Resources/Shaders/quad_vertex_shader.vert",
		"D:/Github/XperimentalRenderingEngine/XRE/Source/Resources/Shaders/quad_fragment_shader.frag");


	quadShader->use();
	quadShader->setBool("use_pfx", false);
	postfx = new PostProcessing();

	draw_queue.reserve(50);

	first_draw = true;
	FBO_Color_Attachments[0] = GL_COLOR_ATTACHMENT0;
	FBO_Color_Attachments[1] = GL_COLOR_ATTACHMENT1;

	depthShader = new Shader(
		"D:/Github/XperimentalRenderingEngine/XRE/Source/Resources/Shaders/depth_map_vertex_shader.vert",
		"D:/Github/XperimentalRenderingEngine/XRE/Source/Resources/Shaders/depth_map_fragment_shader.frag",
		"D:/Github/XperimentalRenderingEngine/XRE/Source/Resources/Shaders/depth_map_geometry_shader.geom");

	blurShader = new Shader(
		"D:/Github/XperimentalRenderingEngine/XRE/Source/Resources/Shaders/quad_vertex_shader.vert",
		"D:/Github/XperimentalRenderingEngine/XRE/Source/Resources/Shaders/gaussian_blur_shader.frag"
	);

	directional_light = NULL;
}

void RenderSystem::createQuad()
{
	float quadVertices[] = {
		// positions   // texCoords
		-1.0f,  1.0f,  0.0f, 1.0f,
		-1.0f, -1.0f,  0.0f, 0.0f,
		 1.0f, -1.0f,  1.0f, 0.0f,

		-1.0f,  1.0f,  0.0f, 1.0f,
		 1.0f, -1.0f,  1.0f, 0.0f,
		 1.0f,  1.0f,  1.0f, 1.0f
	};

	glGenVertexArrays(1, &quadVAO);
	glGenBuffers(1, &quadVBO);

	glBindVertexArray(quadVAO);
	glBindBuffer(GL_ARRAY_BUFFER, quadVBO);

	glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
}

void RenderSystem::colorPass() // draw everything to Framebuffer. 2nd pass.
{
	glEnable(GL_DEPTH_TEST);
	glViewport(0, 0, framebuffer_width, framebuffer_height);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	glBindFramebuffer(GL_FRAMEBUFFER, FBO);
	glDrawBuffers(2, &FBO_Color_Attachments[0]);

	std::stringstream ss; //optimize

	for (unsigned int i = 0; i < draw_queue.size(); i++)
	{
		draw_queue[i].object_shader->use();

		draw_queue[i].object_shader->setFloat("near", light_near_plane);
		draw_queue[i].object_shader->setFloat("far", light_far_plane);

		unsigned int j;
		for (j = 0; j < draw_queue[i].object_textures->size(); j++)
		{
			glActiveTexture(GL_TEXTURE0 + j);
			draw_queue[i].object_shader->setInt(draw_queue[i].object_textures->at(j).type, j);
			glBindTexture(GL_TEXTURE_2D, draw_queue[i].object_textures->at(j).id);
		}

		for (unsigned int k = j; k < point_lights.size() + j; k++)
		{
			ss.str("");
			ss << "[" << k - j + 1 << "]";

			draw_queue[i].object_shader->setVec3("light_position_vertex" + ss.str(), point_lights[k - j]->m_position);

			ss.str("");
			ss << "[" << k - j << "]";
			glActiveTexture(GL_TEXTURE0 + k);
			draw_queue[i].object_shader->setInt("point_shadow_framebuffer_depth_texture_cubemap_static" + ss.str(), k);
			glBindTexture(GL_TEXTURE_CUBE_MAP, point_shadow_framebuffer_depth_texture_cubemap_static[k - j]);
		}

		j += point_lights.size();

		for (unsigned int k = j; k < point_lights.size() + j; k++)
		{
			ss.str("");
			ss << "[" << k - j << "]";
			glActiveTexture(GL_TEXTURE0 + k);
			draw_queue[i].object_shader->setInt("point_shadow_framebuffer_depth_texture_cubemap_dynamic" + ss.str(), k);
			glBindTexture(GL_TEXTURE_CUBE_MAP, point_shadow_framebuffer_depth_texture_cubemap_dynamic[k - j]);
		}

		j += point_lights.size();

		if (directional_light != NULL)
		{
			draw_queue[i].object_shader->setMat4("directional_light_space_matrix", directional_light_space_matrix);
			draw_queue[i].object_shader->setVec3("directional_light_position", directional_light->m_position);

			glActiveTexture(GL_TEXTURE0 + j);
			draw_queue[i].object_shader->setInt("shadow_depth_map_directional", j);
			glBindTexture(GL_TEXTURE_2D, directional_shadow_framebuffer_depth_texture);

			draw_queue[i].object_shader->setVec3("light_position_vertex[0]", directional_light->m_position);
		}

		for (unsigned int l = 0; l < lights.size(); l++)
		{
			lights[l]->SetShaderAttrib(lights[l]->m_name, *draw_queue[i].object_shader);
		}

		glBindVertexArray(draw_queue[i].object_VAO);
		glDrawElements(GL_TRIANGLES, draw_queue[i].indices_size, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}

	glDisable(GL_CULL_FACE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderSystem::directionalShadowPass()
{
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);
	glViewport(0, 0, shadow_map_width * 4, 4 * shadow_map_height);

	depthShader->use();

	if (directional_light != NULL)
	{
		glViewport(0, 0, shadow_map_width * 2, 2 * shadow_map_height);
		glBindFramebuffer(GL_FRAMEBUFFER, directional_shadow_framebuffer);
		depthShader->setInt("faces", 1);

		createDirectionalLightMatrix(directional_light->m_position, directional_light->m_direction);

		depthShader->setMat4("light_space_matrix_cube[0]", directional_light_space_matrix);
		depthShader->setVec3("lightPos", directional_light->m_position);
		depthShader->setFloat("farPlane", light_far_plane);

		for (unsigned int i = 0; i < draw_queue.size(); i++)
		{
			depthShader->setMat4("model", *draw_queue[i].object_model_matrix);
			glBindVertexArray(draw_queue[i].object_VAO);
			glDrawElements(GL_TRIANGLES, draw_queue[i].indices_size, GL_UNSIGNED_INT, 0);
			glBindVertexArray(0);
		}
	}

	glDisable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glDisable(GL_DEPTH_TEST);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderSystem::pointShadowPass()
{
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);
	glViewport(0, 0, shadow_map_width, shadow_map_height);

	depthShader->use();
	depthShader->setInt("faces", 6);

	for (unsigned int k = 0; k < point_lights.size(); k++)
	{
		createPointLightMatrices(point_lights[k]->m_position, k);

		depthShader->setMat4("light_space_matrix_cube[0]", point_light_space_matrix_cube_array[k].point_light_space_matrix_0);
		depthShader->setMat4("light_space_matrix_cube[1]", point_light_space_matrix_cube_array[k].point_light_space_matrix_1);
		depthShader->setMat4("light_space_matrix_cube[2]", point_light_space_matrix_cube_array[k].point_light_space_matrix_2);
		depthShader->setMat4("light_space_matrix_cube[3]", point_light_space_matrix_cube_array[k].point_light_space_matrix_3);
		depthShader->setMat4("light_space_matrix_cube[4]", point_light_space_matrix_cube_array[k].point_light_space_matrix_4);
		depthShader->setMat4("light_space_matrix_cube[5]", point_light_space_matrix_cube_array[k].point_light_space_matrix_5);

		depthShader->setVec3("lightPos", point_lights[k]->m_position);
		depthShader->setFloat("farPlane", light_far_plane);

		if (first_draw)
		{
			glViewport(0, 0, shadow_map_width, shadow_map_height);
			glBindFramebuffer(GL_FRAMEBUFFER, point_shadow_framebuffer_static[k]);
			glClear(GL_DEPTH_BUFFER_BIT);

			for (unsigned int i = 0; i < draw_queue.size(); i++)
			{
				if (draw_queue[i].dynamic)
				{
					continue;
				}

				depthShader->setMat4("model", *draw_queue[i].object_model_matrix);
				glBindVertexArray(draw_queue[i].object_VAO);
				glDrawElements(GL_TRIANGLES, draw_queue[i].indices_size, GL_UNSIGNED_INT, 0);
				glBindVertexArray(0);
			}

		}

		glBindFramebuffer(GL_FRAMEBUFFER, point_shadow_framebuffer_dynamic[k]);
		glViewport(0, 0, shadow_map_width * 2, 2 * shadow_map_height);

		for (unsigned int i = 0; i < draw_queue.size(); i++)
		{
			if (!draw_queue[i].dynamic)
			{
				continue;
			}

			depthShader->setMat4("model", *draw_queue[i].object_model_matrix);
			glBindVertexArray(draw_queue[i].object_VAO);
			glDrawElements(GL_TRIANGLES, draw_queue[i].indices_size, GL_UNSIGNED_INT, 0);
			glBindVertexArray(0);
		}
	}
	first_draw = false;

	glDisable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glDisable(GL_DEPTH_TEST);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderSystem::drawToScreen()
{
	clearPointShadowFramebufferDynamic();
	pointShadowPass();

	clearDirectionalShadowMapFramebuffer();
	directionalShadowPass();

	clearRenderFramebuffer();
	colorPass();

	clearPingPongBuffers();
	blurPass(); // blooooom....

	glViewport(0, 0, framebuffer_width, framebuffer_height);

	clearDefaultFramebuffer();
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindVertexArray(quadVAO);
	glDisable(GL_DEPTH_TEST);

	quadShader->use();

	glActiveTexture(GL_TEXTURE0);
	quadShader->setInt("screenTexture", 0);
	glBindTexture(GL_TEXTURE_2D, FBO_primary_texture);

	glActiveTexture(GL_TEXTURE1);
	quadShader->setInt("blurTexture", 1);
	glBindTexture(GL_TEXTURE_2D, PingPong_textures[0]);

	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);
}

void RenderSystem::draw(unsigned int vertex_array_object, unsigned int indices_size, const xre::Shader& object_shader, const glm::mat4& model_matrix, std::vector<Texture>* object_textures, std::vector<std::string>* texture_types, std::string model_name, bool is_dynamic, bool* setup_success)
{
	model_information model_info_i;
	model_info_i.object_VAO = vertex_array_object;
	model_info_i.indices_size = indices_size;
	model_info_i.object_shader = &(object_shader);
	model_info_i.object_model_matrix = &(model_matrix);
	model_info_i.object_textures = object_textures;
	model_info_i.model_name = model_name;
	model_info_i.texture_types = texture_types;
	model_info_i.setup_success = setup_success;
	model_info_i.dynamic = is_dynamic;

	*setup_success = true;

	draw_queue.push_back(model_info_i);
}

void RenderSystem::createFramebuffers()
{
	// Main Framebuffer
	glGenFramebuffers(1, &FBO);
	glBindFramebuffer(GL_FRAMEBUFFER, FBO);

	// Primary Texture
	glGenTextures(1, &FBO_primary_texture);
	glBindTexture(GL_TEXTURE_2D, FBO_primary_texture);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, framebuffer_width, framebuffer_height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, FBO_primary_texture, 0);

	// Secondary Texture
	glGenTextures(1, &FBO_secondary_texture);
	glBindTexture(GL_TEXTURE_2D, FBO_secondary_texture);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, framebuffer_width, framebuffer_height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, FBO_secondary_texture, 0);

	// Renderbuffer

	glGenRenderbuffers(1, &RBO);
	glBindRenderbuffer(GL_RENDERBUFFER, RBO);

	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, framebuffer_width, framebuffer_height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, RBO);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
	{
		LOGGER->log(INFO, "Render System : createFramebuffer", "Framebuffer complete.");
	}
	else
	{
		LOGGER->log(ERROR, "Render System : createFramebuffer", "Framebuffer incomplete!");
	}

	glBindRenderbuffer(GL_RENDERBUFFER, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	clearRenderFramebuffer();

	// Ping-Pong Framebuffers

	glGenFramebuffers(2, PingPongFBO);
	glGenTextures(2, PingPong_textures);

	for (unsigned int i = 0; i < 2; i++)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, PingPongFBO[i]);
		glBindTexture(GL_TEXTURE_2D, PingPong_textures[i]);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, framebuffer_width / 2, framebuffer_height / 2, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, PingPong_textures[i], 0);
	}

	clearPingPongBuffers();
}

void RenderSystem::clearRenderFramebuffer()
{
	glBindFramebuffer(GL_FRAMEBUFFER, FBO);

	glClearColor(bg_color.x, bg_color.y, bg_color.z, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderSystem::clearDefaultFramebuffer()
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glClearColor(bg_color.x, bg_color.y, bg_color.z, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
}

void RenderSystem::clearDirectionalShadowMapFramebuffer()
{
	glBindFramebuffer(GL_FRAMEBUFFER, directional_shadow_framebuffer);
	glClear(GL_DEPTH_BUFFER_BIT);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderSystem::clearPointShadowFramebufferStatic()
{
	for (unsigned int i = 0; i < 5; i++)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, point_shadow_framebuffer_static[i]);
		glClear(GL_DEPTH_BUFFER_BIT);
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderSystem::clearPointShadowFramebufferDynamic()
{
	for (unsigned int i = 0; i < 5; i++)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, point_shadow_framebuffer_dynamic[i]);
		glClear(GL_DEPTH_BUFFER_BIT);
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderSystem::createShadowMapFramebuffer()
{
	// directional
	glGenFramebuffers(1, &directional_shadow_framebuffer);

	glGenTextures(1, &directional_shadow_framebuffer_depth_texture);
	glBindTexture(GL_TEXTURE_2D, directional_shadow_framebuffer_depth_texture);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, shadow_map_width * 2, 2 * shadow_map_height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

	float border_color[] = { 0.0f,0.0f,0.0f,1.0f };

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_color);

	glBindFramebuffer(GL_FRAMEBUFFER, directional_shadow_framebuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, directional_shadow_framebuffer_depth_texture, 0);

	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		LOGGER->log(ERROR, "Renderer : createShadowMapBuffer", "Directional Shadow Framebuffer is incomplete!");

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// point - Static
	glGenFramebuffers(5, &point_shadow_framebuffer_static[0]);

	glGenTextures(5, &point_shadow_framebuffer_depth_texture_cubemap_static[0]);

	for (unsigned int i = 0; i < 5; i++)
	{
		glBindTexture(GL_TEXTURE_CUBE_MAP, point_shadow_framebuffer_depth_texture_cubemap_static[i]);
		for (unsigned int i = 0; i < 6; i++)
		{
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT32F, shadow_map_width, shadow_map_height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
		}

		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		glBindFramebuffer(GL_FRAMEBUFFER, point_shadow_framebuffer_static[i]);

		glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, point_shadow_framebuffer_depth_texture_cubemap_static[i], 0);

		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			LOGGER->log(ERROR, "Renderer : createShadowMapBuffer", "Point Shadow Framebuffer is incomplete!");
	}

	// point - dynamic
	glGenFramebuffers(5, &point_shadow_framebuffer_dynamic[0]);

	glGenTextures(5, &point_shadow_framebuffer_depth_texture_cubemap_dynamic[0]);

	for (unsigned int i = 0; i < 5; i++)
	{
		glBindTexture(GL_TEXTURE_CUBE_MAP, point_shadow_framebuffer_depth_texture_cubemap_dynamic[i]);
		for (unsigned int i = 0; i < 6; i++)
		{
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT32F, shadow_map_width * 2, shadow_map_height * 2, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
		}

		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		glBindFramebuffer(GL_FRAMEBUFFER, point_shadow_framebuffer_dynamic[i]);

		glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, point_shadow_framebuffer_depth_texture_cubemap_dynamic[i], 0);

		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			LOGGER->log(ERROR, "Renderer : createShadowMapBuffer", "Point Shadow Framebuffer is incomplete!");
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderSystem::clearPingPongBuffers()
{
	for (unsigned int i = 0; i < 2; i++)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, PingPongFBO[i]);
		glClear(GL_COLOR_BUFFER_BIT);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderSystem::createDirectionalLightMatrix(glm::vec3 light_position, glm::vec3 light_front)
{
	directional_light_projection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, light_near_plane, light_far_plane);
	directional_light_space_matrix = directional_light_projection * glm::lookAt(light_position, glm::vec3(0.0f), glm::cross(xre::WORLD_RIGHT, light_front));
}

void RenderSystem::createPointLightMatrices(glm::vec3 light_position, unsigned int light_index)
{
	point_light_space_matrix_cube plsmc;
	point_light_projection = glm::perspective(glm::radians(90.0f), 1.0f, light_near_plane, light_far_plane);

	plsmc.point_light_space_matrix_0 = point_light_projection * glm::lookAt(light_position, light_position + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f));
	plsmc.point_light_space_matrix_1 = point_light_projection * glm::lookAt(light_position, light_position + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f));
	plsmc.point_light_space_matrix_2 = point_light_projection * glm::lookAt(light_position, light_position + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	plsmc.point_light_space_matrix_3 = point_light_projection * glm::lookAt(light_position, light_position + glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f));
	plsmc.point_light_space_matrix_4 = point_light_projection * glm::lookAt(light_position, light_position + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f));
	plsmc.point_light_space_matrix_5 = point_light_projection * glm::lookAt(light_position, light_position + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f));

	point_light_space_matrix_cube_array[light_index] = plsmc;
}

void RenderSystem::addToRenderSystem(Light* light)
{
	if (typeid(*light) == typeid(DirectionalLight))
	{
		if (directional_light == NULL)
		{
			directional_light = (DirectionalLight*)light;
		}
		else
		{
			LOGGER->log(ERROR, " RenderSystem::addToRenderSystem(Light* light)", "Cannot have more than 1 directional light.");
		}
	}
	else
	{
		point_lights.push_back((PointLight*)light);
	}
	lights.push_back(light);
}

void RenderSystem::setCameraMatrices(const glm::mat4* view, const glm::mat4* projection, const glm::vec3* position)
{
	camera_view_matrix = view;
	camera_projection_matrix = projection;
	camera_position = position;
}

void RenderSystem::blurPass()
{
	glDisable(GL_DEPTH_TEST);

	unsigned int amount = 5;
	blurShader->use();
	first_iteration = true;

	horizontal = true;

	for (unsigned int i = 0; i < amount; i++)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, PingPongFBO[horizontal]);
		blurShader->setBool("horizontal", horizontal);

		glViewport(0, 0, framebuffer_width / 2, framebuffer_height / 2);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, first_iteration ? FBO_secondary_texture : PingPong_textures[!horizontal]);

		glBindVertexArray(quadVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindVertexArray(0);

		horizontal = !horizontal;

		if (first_iteration)
			first_iteration = false;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderSystem::SwitchPfx(bool option)
{
	quadShader->setBool("use_pfx", option);
}

void RenderSystem::PostProcessing::updateBloom(bool* const enabled, float* const intensity, float* const threshold, float* const radius, glm::vec3* const color)
{
	instance->quadShader->use();

	if (enabled != NULL)
	{
		instance->quadShader->setBool("bloom_options_i.enabled", *enabled);
		instance->postfx->bloom_options_i.enabled = *enabled;
	}
	if (intensity != NULL)
	{
		instance->quadShader->setFloat("bloom_options_i.intensity", *intensity);
		instance->postfx->bloom_options_i.intensity = *intensity;
	}

	if (threshold != NULL)
	{
		instance->quadShader->setFloat("bloom_options_i.threshold", *threshold);
		instance->postfx->bloom_options_i.threshold = *threshold;
	}

	if (radius != NULL)
	{
		instance->quadShader->setFloat("bloom_options_i.radius", *radius);
		instance->postfx->bloom_options_i.radius = *radius;
	}

	if (color != NULL)
	{
		instance->quadShader->setVec3("bloom_options_i.color", *color);
		instance->postfx->bloom_options_i.color = *color;
	}
}

void RenderSystem::PostProcessing::updateBlur(bool* const enabled, float* const intensity, float* const radius, int* const kernel_size)
{
	instance->quadShader->use();

	if (enabled != NULL)
	{
		instance->quadShader->setBool("bloom_options_i.enabled", *enabled);
		instance->postfx->bloom_options_i.enabled = *enabled;
	}
	if (intensity != NULL)
	{
		instance->quadShader->setFloat("bloom_options_i.intensity", *intensity);
		instance->postfx->bloom_options_i.intensity = *intensity;
	}

	if (radius != NULL)
	{
		instance->quadShader->setFloat("bloom_options_i.radius", *radius);
		instance->postfx->bloom_options_i.radius = *radius;
	}
}

void RenderSystem::PostProcessing::updateSSAO(bool* const enabled, float* const intensity, float* const radius, glm::vec3* const color)
{
	instance->quadShader->use();

	if (enabled != NULL)
	{
		instance->quadShader->setBool("bloom_options_i.enabled", *enabled);
		instance->postfx->bloom_options_i.enabled = *enabled;
	}
	if (intensity != NULL)
	{
		instance->quadShader->setFloat("bloom_options_i.intensity", *intensity);
		instance->postfx->bloom_options_i.intensity = *intensity;
	}

	if (radius != NULL)
	{
		instance->quadShader->setFloat("bloom_options_i.radius", *radius);
		instance->postfx->bloom_options_i.radius = *radius;
	}

	if (color != NULL)
	{
		instance->quadShader->setVec3("bloom_options_i.color", *color);
		instance->postfx->bloom_options_i.color = *color;
	}
}