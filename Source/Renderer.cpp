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
	createRenderFramebuffer();
	createShadowMapFramebuffer();
	createQuad();
	quadShader = new Shader("D:/Work/Xperimental Rendering Engine/XRE/Source/Resources/Shaders/quad_vertex_shader.vert", "D:/Work/Xperimental Rendering Engine/XRE/Source/Resources/Shaders/quad_fragment_shader.frag");
	quadShader->use();
	quadShader->setBool("use_pfx", false);
	postfx = new PostProcessing();

	depthShader = new Shader(
		"D:/Work/Xperimental Rendering Engine/XRE/Source/Resources/Shaders/depth_map_vertex_shader.vert",
		"D:/Work/Xperimental Rendering Engine/XRE/Source/Resources/Shaders/depth_map_fragment_shader.frag",
		"D:/Work/Xperimental Rendering Engine/XRE/Source/Resources/Shaders/depth_map_geometry_shader.geom");
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

	glBindFramebuffer(GL_FRAMEBUFFER, FBO);

	std::stringstream ss;

	for (unsigned int i = 0; i < draw_queue.size(); i++)
	{
		unsigned int j;
		for (j = 0; j < draw_queue[i].object_textures->size(); j++)
		{
			glActiveTexture(GL_TEXTURE0 + j);
			draw_queue[i].object_shader->setInt(draw_queue[i].object_textures->at(j).type, j);
			glBindTexture(GL_TEXTURE_2D, draw_queue[i].object_textures->at(j).id);
		}
		draw_queue[i].object_shader->setInt("directional_shadow_textures_count", directional_shadow_textures.size());
		draw_queue[i].object_shader->setInt("point_shadow_textures_count", point_shadow_textures.size());

		for (unsigned int k = j; k < point_shadow_textures.size() + j; k++)
		{
			ss.str("");
			ss << "[" << k - j << "]";

			draw_queue[i].object_shader->setVec3("pointLightPos", point_lights[k - j]->m_position);

			glActiveTexture(GL_TEXTURE0 + k);
			draw_queue[i].object_shader->setInt("shadow_depth_map_cubemap" + ss.str(), k);
			glBindTexture(GL_TEXTURE_CUBE_MAP, point_shadow_textures[k - j].id);
		}
		j += point_shadow_textures.size();

		draw_queue[i].object_shader->setMat4("directional_light_space_matrix", directional_light_space_matrix);
		for (unsigned int k = j; k < directional_shadow_textures.size() + j; k++)
		{
			ss.str("");
			ss << "[" << k - j << "]";

			glActiveTexture(GL_TEXTURE0 + k);
			draw_queue[i].object_shader->setInt("shadow_depth_map_directional" + ss.str(), k);

			draw_queue[i].object_shader->setVec3("light_position_vertex", directional_lights[k - j]->m_position);

			glBindTexture(GL_TEXTURE_2D, directional_shadow_textures[k - j].id);
		}

		for (unsigned int l = 0; l < lights.size(); l++)
		{
			lights[l]->SetShaderAttrib(lights[l]->m_name, *draw_queue[i].object_shader);
		}

		glBindVertexArray(draw_queue[i].object_VAO);
		glDrawElements(GL_TRIANGLES, draw_queue[i].indices_size, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	directional_shadow_textures.clear();
	point_shadow_textures.clear();
	point_light_space_matrix_cube_array.clear();
}

// Draw call in the shadow pass is made twice. Fix that!
void RenderSystem::shadowPass() // draw depth only. 1st pass.
{
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);
	glViewport(0, 0, shadow_map_width, shadow_map_height);

	Texture shadow_texture;

	depthShader->setFloat("far_plane", light_far_plane);

	glBindFramebuffer(GL_FRAMEBUFFER, directional_shadow_framebuffer);

	depthShader->setBool("is_directional", true);
	for (unsigned int j = 0; j < directional_lights.size(); j++) // we can work with just one directional light at the moment.
	{
		createDirectionalLightMatrix(directional_lights[j]->m_position, directional_lights[j]->m_direction);

		depthShader->setMat4("directional_light_space_matrix", directional_light_space_matrix);

		for (unsigned int i = 0; i < draw_queue.size(); i++)
		{
			depthShader->setVec3("light_pos", directional_lights[j]->m_position);
			depthShader->setMat4("model", *draw_queue[i].object_model_matrix);
			glBindVertexArray(draw_queue[i].object_VAO);
			glDrawElements(GL_TRIANGLES, draw_queue[i].indices_size, GL_UNSIGNED_INT, 0);
			glBindVertexArray(0);
		}

		shadow_texture.id = directional_shadow_framebuffer_depth_texture;
		shadow_texture.path = "";
		shadow_texture.type = "shadow_depth_map_directional";

		directional_shadow_textures.push_back(shadow_texture);
	}

	depthShader->setBool("is_directional", point_lights.size() == 0);

	glBindFramebuffer(GL_FRAMEBUFFER, point_shadow_framebuffer);

	for (unsigned int k = 0; k < point_lights.size(); k++)
	{
		createPointLightMatrices(point_lights[k]->m_position);

		depthShader->setMat4("point_light_space_matrix_cube[0]", point_light_space_matrix_cube_array[k].point_light_space_matrix_0);
		depthShader->setMat4("point_light_space_matrix_cube[1]", point_light_space_matrix_cube_array[k].point_light_space_matrix_1);
		depthShader->setMat4("point_light_space_matrix_cube[2]", point_light_space_matrix_cube_array[k].point_light_space_matrix_2);
		depthShader->setMat4("point_light_space_matrix_cube[3]", point_light_space_matrix_cube_array[k].point_light_space_matrix_3);
		depthShader->setMat4("point_light_space_matrix_cube[4]", point_light_space_matrix_cube_array[k].point_light_space_matrix_4);
		depthShader->setMat4("point_light_space_matrix_cube[5]", point_light_space_matrix_cube_array[k].point_light_space_matrix_5);

		depthShader->setVec3("light_pos", point_lights[k]->m_position);

		for (unsigned int i = 0; i < draw_queue.size(); i++)
		{
			depthShader->setVec3("light_pos", point_lights[k]->m_position);
			depthShader->setMat4("model", *draw_queue[i].object_model_matrix);
			glBindVertexArray(draw_queue[i].object_VAO);
			glDrawElements(GL_TRIANGLES, draw_queue[i].indices_size, GL_UNSIGNED_INT, 0);
			glBindVertexArray(0);
		}

		shadow_texture.id = point_shadow_framebuffer_depth_texture_cubemap;
		shadow_texture.path = "";
		shadow_texture.type = "shadow_depth_map_cubemap";

		point_shadow_textures.push_back(shadow_texture);
	}

	glDisable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glDisable(GL_DEPTH_TEST);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderSystem::drawToScreen()
{
	clearShadowMapFramebuffer();
	shadowPass();

	clearRenderFramebuffer();
	colorPass();

	getWorldViewPos();

	clearDefaultFramebuffer();
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindVertexArray(quadVAO);
	glDisable(GL_DEPTH_TEST);
	//ewfewipfu
	quadShader->use();
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, FBO_texture);
	//glViewport(0, 0, shadow_map_width, shadow_map_height); glBindTexture(GL_TEXTURE_CUBE_MAP, point_shadow_framebuffer_depth_texture_cubemap);
	glDrawArrays(GL_TRIANGLES, 0, 6);

	draw_queue.clear();
}

void RenderSystem::draw(unsigned int vertex_array_object, unsigned int indices_size, const xre::Shader& object_shader, const glm::mat4& model_matrix, std::vector<Texture>* object_textures, std::vector<std::string>* texture_types, std::string model_name, bool* setup_success)
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

	*setup_success = true;

	draw_queue.push_back(model_info_i);
}

void RenderSystem::createRenderFramebuffer()
{
	glGenFramebuffers(1, &FBO);
	glBindFramebuffer(GL_FRAMEBUFFER, FBO);

	glGenTextures(1, &FBO_texture);
	glBindTexture(GL_TEXTURE_2D, FBO_texture);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, framebuffer_width, framebuffer_height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, FBO_texture, 0);

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


	// Create Pixel Buffer Objects
	glGenBuffers(1, &PBOS[0]);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, PBOS[0]);
	glBufferData(GL_PIXEL_PACK_BUFFER, sizeof(GLfloat) * 1, NULL, GL_STREAM_READ);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

	glGenBuffers(1, &PBOS[1]);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, PBOS[1]);
	glBufferData(GL_PIXEL_PACK_BUFFER, sizeof(GLfloat) * 1, NULL, GL_STREAM_READ);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

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

void RenderSystem::clearShadowMapFramebuffer()
{
	glBindFramebuffer(GL_FRAMEBUFFER, directional_shadow_framebuffer);
	glClear(GL_DEPTH_BUFFER_BIT);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, point_shadow_framebuffer);
	glClear(GL_DEPTH_BUFFER_BIT);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderSystem::createShadowMapFramebuffer()
{
	// directional
	glGenFramebuffers(1, &directional_shadow_framebuffer);

	glGenTextures(1, &directional_shadow_framebuffer_depth_texture);
	glBindTexture(GL_TEXTURE_2D, directional_shadow_framebuffer_depth_texture);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, shadow_map_width, shadow_map_height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

	float border_color[] = { 1.0f,1.0f,1.0f,1.0f };

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

	// point
	glGenFramebuffers(1, &point_shadow_framebuffer);

	glGenTextures(1, &point_shadow_framebuffer_depth_texture_cubemap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, point_shadow_framebuffer_depth_texture_cubemap);

	for (unsigned int i = 0; i < 6; i++)
	{
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT, shadow_map_width, shadow_map_height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	}

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	glBindFramebuffer(GL_FRAMEBUFFER, point_shadow_framebuffer);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, point_shadow_framebuffer_depth_texture_cubemap, 0);

	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		LOGGER->log(ERROR, "Renderer : createShadowMapBuffer", "Point Shadow Framebuffer is incomplete!");

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderSystem::createDirectionalLightMatrix(glm::vec3 light_position, glm::vec3 light_front)
{
	directional_light_projection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, light_near_plane, light_far_plane);
	directional_light_space_matrix = directional_light_projection * glm::lookAt(light_position, glm::vec3(0.0f), glm::cross(xre::WORLD_RIGHT, light_front));
}

void RenderSystem::createPointLightMatrices(glm::vec3 light_position)
{
	point_light_space_matrix_cube plsmc;

	point_light_projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 50.0f);

	plsmc.point_light_space_matrix_0 = point_light_projection * glm::lookAt(light_position, light_position + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f));
	plsmc.point_light_space_matrix_1 = point_light_projection * glm::lookAt(light_position, light_position + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f));
	plsmc.point_light_space_matrix_2 = point_light_projection * glm::lookAt(light_position, light_position + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	plsmc.point_light_space_matrix_3 = point_light_projection * glm::lookAt(light_position, light_position + glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f));
	plsmc.point_light_space_matrix_4 = point_light_projection * glm::lookAt(light_position, light_position + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f));
	plsmc.point_light_space_matrix_5 = point_light_projection * glm::lookAt(light_position, light_position + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f));

	point_light_space_matrix_cube_array.push_back(plsmc);
}

void RenderSystem::addToRenderSystem(Light* light)
{
	if (typeid(*light) == typeid(DirectionalLight))
	{
		directional_lights.push_back((DirectionalLight*)light);
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

void RenderSystem::getWorldViewPos()
{
	pbo_current_index = (pbo_current_index + 1) % 2;
	pbo_next_index = (pbo_current_index + 1) % 2;

	glBindFramebuffer(GL_FRAMEBUFFER, FBO);
	glReadBuffer(GL_DEPTH_ATTACHMENT);

	glBindBuffer(GL_PIXEL_PACK_BUFFER, PBOS[pbo_current_index]);

	glReadPixels(framebuffer_width * sample_coords[0].x, framebuffer_height * sample_coords[0].y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, PBOS[pbo_next_index]);

	world_view_pos = glm::vec3(0.0);
	for (unsigned int i = 0; i < 1; i++)
	{
		depth = (GLfloat*)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
		float d = 0.0;
		if (depth)
		{
			d = 2.0 * (*depth) - 1.0;
			d = 2.0 * 0.1 * 100.0 / (100.0 + 0.1 - d * (100.0 - 0.1));

			glm::vec4 clip_pos = glm::vec4(sample_coords[i].x * 2.0 - 1.0, sample_coords[i].y * 2.0 - 1.0, (*depth) * 2.0 - 1.0, 1.0);
			glm::vec4 view_pos = glm::inverse((*camera_projection_matrix)) * clip_pos;

			view_pos /= view_pos.w;

			glm::vec4 world_pos = glm::inverse((*camera_view_matrix)) * view_pos;

			world_view_pos += (*camera_position) + glm::normalize(glm::vec3(world_pos.x, world_pos.y, world_pos.z)) * d;

			glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
		}
	}
	//std::cout << world_view_pos.x << "," << world_view_pos.y << "," << world_view_pos.z << "\n";

	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
}

void RenderSystem::SwitchPfx(bool option)
{
	quadShader->setBool("use_pfx", option);
}

void RenderSystem::setPull(float p)
{
	pull_strength = p;
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