#include <renderer.h>

#include <iostream>
#include <string>
#include <sstream>
#include <random>


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
RenderSystem* RenderSystem::renderer(unsigned int screen_width, unsigned int screen_height, const glm::vec4& background_color, float lights_near_plane_p, float lights_far_plane_p, int shadow_map_width_p, int shadow_map_height_p, RENDER_PIPELINE render_pipeline, LIGHTING_MODE light_mode)
{
	if (instance == NULL)
	{
		instance = new RenderSystem(screen_width, screen_height, background_color, lights_near_plane_p, lights_far_plane_p, shadow_map_width_p, shadow_map_height_p, render_pipeline, light_mode);
	}
	return instance;
}

RenderSystem::RenderSystem(unsigned int screen_width, unsigned int screen_height, const glm::vec4& background_color, float lights_near_plane_p, float lights_far_plane_p, int shadow_map_width_p, int shadow_map_height_p, RENDER_PIPELINE render_pipeline, LIGHTING_MODE light_mode)
	:framebuffer_width(screen_width), framebuffer_height(screen_height), bg_color(background_color), light_near_plane(lights_near_plane_p), light_far_plane(lights_far_plane_p), shadow_map_width(shadow_map_height_p), shadow_map_height(shadow_map_height_p)
{
	rendering_pipeline = render_pipeline;
	lighting_model = light_mode;
	bg_color = background_color;

	quadShader = new Shader(
		"./Source/Resources/Shaders/Quad/quad_vertex_shader.vert",
		"./Source/Resources/Shaders/Quad/quad_fragment_shader.frag");
	createQuad();

	if (rendering_pipeline == RENDER_PIPELINE::DEFERRED)
	{
		createDeferredBuffers();
		createShadowMapFramebuffers();


		if (lighting_model == LIGHTING_MODE::BLINNPHONG)
		{
			deferredFillShader = new Shader(
				"./Source/Resources/Shaders/DeferredAdditional/deferred_fill_vertex_shader.vert",
				"./Source/Resources/Shaders/DeferredAdditional/deferred_fill_fragment_shader.frag");

			deferredColorShader = new Shader(
				"./Source/Resources/Shaders/BlinnPhong/deferred_bphong_color_vertex_shader.vert",
				"./Source/Resources/Shaders/BlinnPhong/deferred_bphong_color_fragment_shader.frag");
		}
		else
		{
			deferredFillShader = new Shader(
				"./Source/Resources/Shaders/DeferredAdditional/deferred_fill_vertex_shader.vert",
				"./Source/Resources/Shaders/DeferredAdditional/deferred_fill_pbr_fragment_shader.frag");

			deferredColorShader = new Shader(
				"./Source/Resources/Shaders/BlinnPhong/deferred_bphong_color_vertex_shader.vert",
				"./Source/Resources/Shaders/PBR/deferred_pbr_color_fragment_shader.frag");
		}
	}
	else
	{
		createForwardFramebuffers();
		createShadowMapFramebuffers();
	}

	quadShader->use();
	quadShader->setBool("use_pfx", false);

	draw_queue.reserve(50);

	first_draw = true;

	depthShader_point = new Shader(
		"./Source/Resources/Shaders/ShadowMapping/depth_map_vertex_shader.vert",
		"./Source/Resources/Shaders/ShadowMapping/depth_map_point_fragment_shader.frag",
		"./Source/Resources/Shaders/ShadowMapping/depth_map_geometry_shader.geom");

	depthShader_directional = new Shader(
		"./Source/Resources/Shaders/ShadowMapping/depth_map_vertex_shader.vert",
		"./Source/Resources/Shaders/ShadowMapping/depth_map_directional_fragment_shader.frag",
		"./Source/Resources/Shaders/ShadowMapping/depth_map_geometry_shader.geom");

	bloomSSAO_blur_Shader = new Shader(
		"./Source/Resources/Shaders/Quad/quad_vertex_shader.vert",
		"./Source/Resources/Shaders/Blur/bloom_ssao_blur_shader.frag"
	);

	directional_shadow_blur_Shader = new Shader(
		"./Source/Resources/Shaders/Quad/quad_vertex_shader.vert",
		"./Source/Resources/Shaders/Blur/directional_soft_shadow_shadow.frag"
	);

	SSAOShader = new Shader("./Source/Resources/Shaders/SSAO/ssao_vertex_shader.vert", "./Source/Resources/Shaders/SSAO/ssao_fragment_shader.frag");

	createSSAOData();
	createBlurringFramebuffers();

	directional_light = NULL;
}

void RenderSystem::drawToScreen()
{
	if (rendering_pipeline == RENDER_PIPELINE::DEFERRED)
	{
		if (point_lights.size() > 0)
		{
			clearPointShadowFramebufferDynamic();
			pointShadowPass();
		}

		if (directional_light)
		{
			clearDirectionalShadowMapFramebuffer();
			directionalShadowPass();
		}

		clearDeferredBuffers();
		deferredFillPass();
		SSAOPass();
		deferredColorShader->use();
		deferredColorShader->setBool("use_ssao", true);
		deferredColorPass();

		clearPrimaryBlurringFramebuffers();
		blurPass(DeferredFinal_secondary_texture, SSAOFramebuffer_color, 5); // blooooom....

		directionalSoftShadowPass();

		glViewport(0, 0, framebuffer_width, framebuffer_height);

		clearDefaultFramebuffer();
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glBindVertexArray(quadVAO);
		glDisable(GL_DEPTH_TEST);

		quadShader->use();

		glActiveTexture(GL_TEXTURE0);
		quadShader->setInt("screenTexture", 0);
		glBindTexture(GL_TEXTURE_2D, DeferredFinal_primary_texture);

		glActiveTexture(GL_TEXTURE1);
		quadShader->setInt("bloomTexture", 1);
		glBindTexture(GL_TEXTURE_2D, PrimaryBlurringFramebuffer_bloom_textures[0]);

		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindVertexArray(0);
	}
	else
	{
		if (point_lights.size() > 0)
		{
			clearPointShadowFramebufferDynamic();
			pointShadowPass();
		}

		if (directional_light)
		{
			clearDirectionalShadowMapFramebuffer();
			directionalShadowPass();
		}

		clearForwardFramebuffer();
		ForwardColorPass();

		clearPrimaryBlurringFramebuffers();
		blurPass(ForwardFramebuffer_secondary_texture, ForwardFramebuffer_secondary_texture, 5); // blooooom....

		glViewport(0, 0, framebuffer_width, framebuffer_height);

		clearDefaultFramebuffer();
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glBindVertexArray(quadVAO);
		glDisable(GL_DEPTH_TEST);

		quadShader->use();

		glActiveTexture(GL_TEXTURE0);
		quadShader->setInt("screenTexture", 0);
		glBindTexture(GL_TEXTURE_2D, ForwardFramebuffer_primary_texture);

		glActiveTexture(GL_TEXTURE1);
		quadShader->setInt("bloomTexture", 1);
		glBindTexture(GL_TEXTURE_2D, PrimaryBlurringFramebuffer_bloom_textures[0]);

		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindVertexArray(0);
	}
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

void RenderSystem::createShadowMapFramebuffers()
{
#pragma region Directional Shadow Map
	glGenFramebuffers(1, &directional_shadow_framebuffer);

	glGenTextures(1, &directional_shadow_framebuffer_depth_texture);
	glBindTexture(GL_TEXTURE_2D, directional_shadow_framebuffer_depth_texture);
	glGenerateMipmap(GL_TEXTURE_2D);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, shadow_map_width * 2, 2 * shadow_map_height, 0, GL_RG, GL_FLOAT, NULL);

	float border_color[] = { 0.0f,0.0f,0.0f,1.0f };

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_color);

	glBindFramebuffer(GL_FRAMEBUFFER, directional_shadow_framebuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, directional_shadow_framebuffer_depth_texture, 0);

	glGenRenderbuffers(1, &directional_shadow_framebuffer_renderbuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, directional_shadow_framebuffer_renderbuffer);

	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, shadow_map_width * 2, 2 * shadow_map_height);

	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, directional_shadow_framebuffer_renderbuffer);


	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		LOGGER->log(ERROR, "Renderer : createShadowMapBuffer", "Directional Shadow Framebuffer is incomplete!");

	glBindRenderbuffer(GL_RENDERBUFFER, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

#pragma endregion

#pragma region Point Shadow Map Static

	glGenFramebuffers(5, &point_shadow_framebuffer_static[0]);
	glGenTextures(5, &point_shadow_framebuffer_depth_texture_cubemap_static[0]);
	glGenTextures(5, &point_shadow_framebuffer_depth_color_texture_static[0]);

	for (unsigned int i = 0; i < 5; i++)
	{
		glBindTexture(GL_TEXTURE_CUBE_MAP, point_shadow_framebuffer_depth_color_texture_static[i]);
		for (unsigned int j = 0; j < 6; j++)
		{
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + j, 0, GL_RG16F, shadow_map_width, shadow_map_height, 0, GL_RG, GL_FLOAT, NULL);
		}

		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		glBindFramebuffer(GL_FRAMEBUFFER, point_shadow_framebuffer_static[i]);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, point_shadow_framebuffer_depth_color_texture_static[i], 0);

		glBindTexture(GL_TEXTURE_CUBE_MAP, point_shadow_framebuffer_depth_texture_cubemap_static[i]);
		for (int j = 0; j < 6; j++)
		{
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + j, 0, GL_DEPTH_COMPONENT, shadow_map_width, shadow_map_height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
		}

		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		glBindFramebuffer(GL_FRAMEBUFFER, point_shadow_framebuffer_static[i]);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, point_shadow_framebuffer_depth_texture_cubemap_static[i], 0);

		glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	}
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		LOGGER->log(ERROR, "Renderer :  createFrameBuffers ", "Point Shadow Static Framebuffer is incomplete!");

#pragma endregion

#pragma region Point Shadow Map Dynamic
	glGenFramebuffers(5, &point_shadow_framebuffer_dynamic[0]);
	glGenTextures(5, &point_shadow_framebuffer_depth_texture_cubemap_dynamic[0]);
	glGenTextures(5, &point_shadow_framebuffer_depth_color_texture_dynamic[0]);

	for (unsigned int i = 0; i < 5; i++)
	{
		glBindTexture(GL_TEXTURE_CUBE_MAP, point_shadow_framebuffer_depth_color_texture_dynamic[i]);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, 1);

		for (unsigned int j = 0; j < 6; j++)
		{
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + j, 0, GL_RG, shadow_map_width, shadow_map_height, 0, GL_RG, GL_FLOAT, NULL);
		}

		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		glBindFramebuffer(GL_FRAMEBUFFER, point_shadow_framebuffer_dynamic[i]);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, point_shadow_framebuffer_depth_color_texture_dynamic[i], 0);

		glBindTexture(GL_TEXTURE_CUBE_MAP, point_shadow_framebuffer_depth_texture_cubemap_dynamic[i]);
		for (int j = 0; j < 6; j++)
		{
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + j, 0, GL_DEPTH_COMPONENT, shadow_map_width, shadow_map_height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
		}

		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);


		glBindFramebuffer(GL_FRAMEBUFFER, point_shadow_framebuffer_dynamic[i]);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, point_shadow_framebuffer_depth_texture_cubemap_dynamic[i], 0);

		glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	}
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		LOGGER->log(ERROR, "Renderer :  createFrameBuffers ", "Point Shadow dynamic Framebuffer is incomplete!");
#pragma endregion
}

void RenderSystem::createDeferredBuffers()
{
	// Main Color Buffer

	glGenFramebuffers(1, &DeferredFinalBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, DeferredFinalBuffer);

	glGenTextures(1, &DeferredFinal_primary_texture);
	glBindTexture(GL_TEXTURE_2D, DeferredFinal_primary_texture);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, framebuffer_width, framebuffer_height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, DeferredFinal_primary_texture, 0);

	// ----------------------------------

	glGenTextures(1, &DeferredFinal_secondary_texture);
	glBindTexture(GL_TEXTURE_2D, DeferredFinal_secondary_texture);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, framebuffer_width, framebuffer_height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, DeferredFinal_secondary_texture, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		LOGGER->log(ERROR, "Renderer : DeferredFinalBuffer", "DeferredFinalBuffer is incomplete!");

	DeferredFinal_attachments[0] = GL_COLOR_ATTACHMENT0;
	DeferredFinal_attachments[1] = GL_COLOR_ATTACHMENT1;

	// G-Buffer
	glGenFramebuffers(1, &DeferredDataFrameBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, DeferredDataFrameBuffer);

	// Color Buffer (with alpha)
	glGenTextures(1, &DeferredGbuffer_color);
	glBindTexture(GL_TEXTURE_2D, DeferredGbuffer_color);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, framebuffer_width, framebuffer_height, 0, GL_RGBA, GL_FLOAT, NULL);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, DeferredGbuffer_color, 0);
	// ----------------------------------

	// Model Normal Buffer
	glGenTextures(1, &DeferredGbuffer_model_normal);
	glBindTexture(GL_TEXTURE_2D, DeferredGbuffer_model_normal);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, framebuffer_width, framebuffer_height, 0, GL_RGB, GL_FLOAT, NULL);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, DeferredGbuffer_model_normal, 0);
	// ----------------------------------

	/*
	// Model Tangent Buffer
	glGenTextures(1, &DeferredGbuffer_tangent);
	glBindTexture(GL_TEXTURE_2D, DeferredGbuffer_tangent);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, framebuffer_width, framebuffer_height, 0, GL_RGB, GL_FLOAT, NULL);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, 0);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, DeferredGbuffer_tangent, 0);
	// ----------------------------------
	*/

	// Texture Normal Buffer
	glGenTextures(1, &DeferredGbuffer_texture_normal);
	glBindTexture(GL_TEXTURE_2D, DeferredGbuffer_texture_normal);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, framebuffer_width, framebuffer_height, 0, GL_RGB, GL_FLOAT, NULL);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, DeferredGbuffer_texture_normal, 0);
	// ----------------------------------

	// Position Buffer
	glGenTextures(1, &DeferredGbuffer_position);
	glBindTexture(GL_TEXTURE_2D, DeferredGbuffer_position);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, framebuffer_width, framebuffer_height, 0, GL_RGB, GL_FLOAT, NULL);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, DeferredGbuffer_position, 0);
	// ----------------------------------

	// View Normal Buffer
	glGenTextures(1, &DeferredGbuffer_texture_normal_view);
	glBindTexture(GL_TEXTURE_2D, DeferredGbuffer_texture_normal_view);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, framebuffer_width, framebuffer_height, 0, GL_RGB, GL_FLOAT, NULL);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_2D, DeferredGbuffer_texture_normal_view, 0);
	// ----------------------------------

	// MOR Buffer
	glGenTextures(1, &DeferredGbuffer_texture_mor);
	glBindTexture(GL_TEXTURE_2D, DeferredGbuffer_texture_mor);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, framebuffer_width, framebuffer_height, 0, GL_RGB, GL_FLOAT, NULL);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT5, GL_TEXTURE_2D, DeferredGbuffer_texture_mor, 0);
	// ----------------------------------

	// ---------------------------------------------------------------------------------------------------------------------

	glGenRenderbuffers(1, &DeferredRenderBuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, DeferredRenderBuffer);

	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, framebuffer_width, framebuffer_height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, DeferredRenderBuffer);

	glBindRenderbuffer(GL_RENDERBUFFER, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		LOGGER->log(ERROR, "Renderer : GBuffer", "GBuffer is incomplete!");

	for (unsigned int i = 0; i < 6; i++)
	{
		DeferredFrameBuffer_primary_color_attachments[i] = GL_COLOR_ATTACHMENT0 + i;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

void RenderSystem::clearDeferredBuffers()
{
	glBindFramebuffer(GL_FRAMEBUFFER, DeferredDataFrameBuffer);
	glClearColor(bg_color.x, bg_color.y, bg_color.z, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glBindFramebuffer(GL_FRAMEBUFFER, DeferredFinalBuffer);
	glClear(GL_COLOR_BUFFER_BIT);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderSystem::deferredFillPass()
{
	glBindFramebuffer(GL_FRAMEBUFFER, DeferredDataFrameBuffer);
	glDrawBuffers(6, &DeferredFrameBuffer_primary_color_attachments[0]);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glViewport(0, 0, framebuffer_width, framebuffer_height);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	deferredFillShader->use();

	deferredFillShader->setMat4("view", *camera_view_matrix);
	deferredFillShader->setMat4("projection", *camera_projection_matrix);
	deferredFillShader->setVec3("camera_position_vertex", *camera_position);

	for (unsigned int i = 0; i < draw_queue.size(); i++)
	{
		for (unsigned int j = 0; j < draw_queue[i].object_textures->size(); j++)
		{
			glActiveTexture(GL_TEXTURE0 + j);
			deferredFillShader->setInt(draw_queue[i].object_textures->at(j).type, j);
			glBindTexture(GL_TEXTURE_2D, draw_queue[i].object_textures->at(j).id);
		}

		deferredFillShader->setMat4("model", *draw_queue[i].object_model_matrix);

		glBindVertexArray(draw_queue[i].object_VAO);
		glDrawElements(GL_TRIANGLES, draw_queue[i].indices_size, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}

	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderSystem::deferredColorPass()
{
	glBindFramebuffer(GL_FRAMEBUFFER, DeferredFinalBuffer);
	glDrawBuffers(2, &DeferredFinal_attachments[0]);

	glDisable(GL_DEPTH_TEST);
	glViewport(0, 0, framebuffer_width, framebuffer_height);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	glActiveTexture(GL_TEXTURE0);
	deferredColorShader->setInt("diffuse_texture", 0);
	glBindTexture(GL_TEXTURE_2D, DeferredGbuffer_color);

	glActiveTexture(GL_TEXTURE1);
	deferredColorShader->setInt("model_normal_texture", 1);
	glBindTexture(GL_TEXTURE_2D, DeferredGbuffer_model_normal);

	glActiveTexture(GL_TEXTURE2);
	deferredColorShader->setInt("texture_normal_texture", 2);
	glBindTexture(GL_TEXTURE_2D, DeferredGbuffer_texture_normal);

	glActiveTexture(GL_TEXTURE3);
	deferredColorShader->setInt("position_texture", 3);
	glBindTexture(GL_TEXTURE_2D, DeferredGbuffer_position);

	glActiveTexture(GL_TEXTURE4);
	deferredColorShader->setInt("ssao_texture", 4);
	glBindTexture(GL_TEXTURE_2D, PrimaryBlurringFramebuffer_ssao_textures[0]);

	glActiveTexture(GL_TEXTURE5);
	deferredColorShader->setInt("mor_texture", 5);
	glBindTexture(GL_TEXTURE_2D, DeferredGbuffer_texture_mor);

	deferredColorShader->setVec3("camera_pos", *camera_position);
	deferredColorShader->setFloat("near", light_near_plane);
	deferredColorShader->setFloat("far", light_far_plane);
	deferredColorShader->setBool("directional_lighting_enabled", directional_light != NULL);
	deferredColorShader->setInt("N_POINT", point_lights.size());

	if (directional_light != NULL)
	{
		glActiveTexture(GL_TEXTURE6);
		glBindTexture(GL_TEXTURE_2D, DirectionalShadowBlurring_soft_shadow_textures[0]);
		deferredColorShader->setInt("shadow_depth_map_directional", 6);

		deferredColorShader->setMat4("directional_light_space_matrix", directional_light_space_matrix);
	}

	std::stringstream ss;
	for (unsigned int i = 0; i < point_lights.size(); i++)
	{
		ss.str("");
		ss << '[' << i << ']';

		glActiveTexture(GL_TEXTURE7 + i);
		glBindTexture(GL_TEXTURE_CUBE_MAP, point_shadow_framebuffer_depth_color_texture_static[i]);
		deferredColorShader->setInt("point_shadow_framebuffer_depth_color_texture_static" + ss.str(), 7 + i);
	}

	for (unsigned int i = 0; i < point_lights.size(); i++)
	{
		ss.str("");
		ss << '[' << i << ']';

		glActiveTexture(GL_TEXTURE12 + i);
		glBindTexture(GL_TEXTURE_CUBE_MAP, point_shadow_framebuffer_depth_color_texture_dynamic[i]);
		deferredColorShader->setInt("point_shadow_framebuffer_depth_color_texture_dynamic" + ss.str(), 12 + i);
	}


	for (unsigned int l = 0; l < lights.size(); l++)
	{
		lights[l]->SetShaderAttrib(lights[l]->m_name, *deferredColorShader);
	}

	glBindVertexArray(quadVAO);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);

	glEnable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderSystem::directionalShadowPass()
{
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	depthShader_directional->use();
	depthShader_directional->setInt("is_point", 0);

	if (directional_light != NULL)
	{
		glViewport(0, 0, shadow_map_width * 2, 2 * shadow_map_height);
		glBindFramebuffer(GL_FRAMEBUFFER, directional_shadow_framebuffer);
		depthShader_directional->setInt("faces", 1);

		createDirectionalLightMatrix(directional_light->m_position, directional_light->m_direction);

		depthShader_directional->setMat4("light_space_matrix_cube[0]", directional_light_space_matrix);
		depthShader_directional->setVec3("lightPos", directional_light->m_position);
		depthShader_directional->setFloat("farPlane", light_far_plane);
		depthShader_directional->setMat4("directional_light_space_matrix", directional_light_space_matrix);

		for (unsigned int i = 0; i < draw_queue.size(); i++)
		{
			depthShader_directional->setMat4("model", *draw_queue[i].object_model_matrix);
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

	depthShader_point->use();
	depthShader_point->setInt("faces", 6);
	depthShader_point->setInt("is_point", 1);

	for (unsigned int k = 0; k < point_lights.size(); k++)
	{
		createPointLightMatrices(point_lights[k]->m_position, k);

		depthShader_point->setMat4("light_space_matrix_cube[0]", point_light_space_matrix_cube_array[k].point_light_space_matrix_0);
		depthShader_point->setMat4("light_space_matrix_cube[1]", point_light_space_matrix_cube_array[k].point_light_space_matrix_1);
		depthShader_point->setMat4("light_space_matrix_cube[2]", point_light_space_matrix_cube_array[k].point_light_space_matrix_2);
		depthShader_point->setMat4("light_space_matrix_cube[3]", point_light_space_matrix_cube_array[k].point_light_space_matrix_3);
		depthShader_point->setMat4("light_space_matrix_cube[4]", point_light_space_matrix_cube_array[k].point_light_space_matrix_4);
		depthShader_point->setMat4("light_space_matrix_cube[5]", point_light_space_matrix_cube_array[k].point_light_space_matrix_5);

		depthShader_point->setVec3("lightPos", point_lights[k]->m_position);
		depthShader_point->setFloat("farPlane", light_far_plane);

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

				depthShader_point->setMat4("model", *draw_queue[i].object_model_matrix);
				glBindVertexArray(draw_queue[i].object_VAO);
				glDrawElements(GL_TRIANGLES, draw_queue[i].indices_size, GL_UNSIGNED_INT, 0);
				glBindVertexArray(0);
			}

		}

		glBindFramebuffer(GL_FRAMEBUFFER, point_shadow_framebuffer_dynamic[k]);
		glViewport(0, 0, shadow_map_width, shadow_map_height);

		for (unsigned int i = 0; i < draw_queue.size(); i++)
		{
			if (!draw_queue[i].dynamic)
			{
				continue;
			}

			depthShader_point->setMat4("model", *draw_queue[i].object_model_matrix);
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

void RenderSystem::createForwardFramebuffers()
{
	// Main Framebuffer
	glGenFramebuffers(1, &ForwardFramebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, ForwardFramebuffer);

	// Primary Texture
	glGenTextures(1, &ForwardFramebuffer_primary_texture);
	glBindTexture(GL_TEXTURE_2D, ForwardFramebuffer_primary_texture);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, framebuffer_width, framebuffer_height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ForwardFramebuffer_primary_texture, 0);

	// Secondary Texture
	glGenTextures(1, &ForwardFramebuffer_secondary_texture);
	glBindTexture(GL_TEXTURE_2D, ForwardFramebuffer_secondary_texture);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, framebuffer_width, framebuffer_height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, ForwardFramebuffer_secondary_texture, 0);

	// Renderbuffer

	glGenRenderbuffers(1, &ForwardFramebufferRenderbuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, ForwardFramebufferRenderbuffer);

	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, framebuffer_width, framebuffer_height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, ForwardFramebufferRenderbuffer);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
	{
		LOGGER->log(INFO, "Render System : createFramebuffer", "Framebuffer complete.");
	}
	else
	{
		LOGGER->log(ERROR, "Render System : createFramebuffer", "Framebuffer incomplete!");
	}

	ForwardFramebuffer_Color_Attachments[0] = GL_COLOR_ATTACHMENT0;
	ForwardFramebuffer_Color_Attachments[1] = GL_COLOR_ATTACHMENT1;

	glBindRenderbuffer(GL_RENDERBUFFER, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	clearForwardFramebuffer();
}

void RenderSystem::ForwardColorPass() // draw everything to Framebuffer. 2nd pass.
{
	glEnable(GL_DEPTH_TEST);
	glViewport(0, 0, framebuffer_width, framebuffer_height);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	glBindFramebuffer(GL_FRAMEBUFFER, ForwardFramebuffer);
	glDrawBuffers(2, &ForwardFramebuffer_Color_Attachments[0]);

	std::stringstream ss; //optimize

	for (unsigned int i = 0; i < draw_queue.size(); i++)
	{
		draw_queue[i].object_shader->use();

		draw_queue[i].object_shader->setFloat("near", light_near_plane);
		draw_queue[i].object_shader->setFloat("far", light_far_plane);
		draw_queue[i].object_shader->setBool("directional_lighting_enabled", directional_light != NULL);
		draw_queue[i].object_shader->setInt("N_POINT", point_lights.size());

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
			draw_queue[i].object_shader->setInt("point_shadow_framebuffer_depth_color_texture_static" + ss.str(), k);
			glBindTexture(GL_TEXTURE_CUBE_MAP, point_shadow_framebuffer_depth_color_texture_static[k - j]);
		}

		j += point_lights.size();

		for (unsigned int k = j; k < point_lights.size() + j; k++)
		{
			ss.str("");
			ss << "[" << k - j << "]";
			glActiveTexture(GL_TEXTURE0 + k);
			draw_queue[i].object_shader->setInt("point_shadow_framebuffer_depth_color_texture_dynamic" + ss.str(), k);
			glBindTexture(GL_TEXTURE_CUBE_MAP, point_shadow_framebuffer_depth_color_texture_dynamic[k - j]);
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

void RenderSystem::clearForwardFramebuffer()
{
	glBindFramebuffer(GL_FRAMEBUFFER, ForwardFramebuffer);

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
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderSystem::clearDirectionalShadowBlurringFramebuffers()
{
	for (unsigned int i = 0; i < 2; i++)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, DirectionalShadowBlurringFramebuffers[i]);
		glClearColor(1.0, 1.0, 1.0, 1.0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}
}

void RenderSystem::clearPointShadowFramebufferStatic()
{
	for (unsigned int i = 0; i < 5; i++)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, point_shadow_framebuffer_static[i]);
		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderSystem::clearPointShadowFramebufferDynamic()
{
	for (unsigned int i = 0; i < 5; i++)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, point_shadow_framebuffer_dynamic[i]);
		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderSystem::clearPrimaryBlurringFramebuffers()
{
	glClearColor(bg_color.r, bg_color.g, bg_color.b, 1.0);
	for (unsigned int i = 0; i < 2; i++)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, PrimaryBlurringFramebuffers[i]);
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

void RenderSystem::createBlurringFramebuffers()
{
	// Primary Blurring Framebuffers
	glGenFramebuffers(2, PrimaryBlurringFramebuffers);
	glGenTextures(2, &PrimaryBlurringFramebuffer_bloom_textures[0]);
	glGenTextures(2, &PrimaryBlurringFramebuffer_ssao_textures[0]);

	for (unsigned int i = 0; i < 2; i++)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, PrimaryBlurringFramebuffers[i]);

		glBindTexture(GL_TEXTURE_2D, PrimaryBlurringFramebuffer_bloom_textures[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, framebuffer_width / 2, framebuffer_height / 2, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, PrimaryBlurringFramebuffer_bloom_textures[i], 0);

		glBindTexture(GL_TEXTURE_2D, PrimaryBlurringFramebuffer_ssao_textures[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RG, framebuffer_width / 2, framebuffer_height / 2, 0, GL_RG, GL_UNSIGNED_BYTE, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, PrimaryBlurringFramebuffer_ssao_textures[i], 0);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
		{
			LOGGER->log(INFO, "Render System : createFramebuffer", "PrimaryBlurring Framebuffer complete.");
		}
		else
		{
			LOGGER->log(ERROR, "Render System : createFramebuffer", "PrimaryBlurring Framebuffer incomplete!");
		}
	}


	PrimaryBlurringFramebuffers_attachments[0] = GL_COLOR_ATTACHMENT0;
	PrimaryBlurringFramebuffers_attachments[1] = GL_COLOR_ATTACHMENT1;

	clearPrimaryBlurringFramebuffers();

	// Primary Blurring Framebuffers
	glGenFramebuffers(2, DirectionalShadowBlurringFramebuffers);
	glGenTextures(2, &DirectionalShadowBlurring_soft_shadow_textures[0]);

	for (unsigned int i = 0; i < 2; i++)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, DirectionalShadowBlurringFramebuffers[i]);

		glBindTexture(GL_TEXTURE_2D, DirectionalShadowBlurring_soft_shadow_textures[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, shadow_map_width * 2, 2 * shadow_map_height, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, DirectionalShadowBlurring_soft_shadow_textures[i], 0);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
		{
			LOGGER->log(INFO, "Render System : createFramebuffer", "DirectionalShadowBlurring Framebuffer complete.");
		}
		else
		{
			LOGGER->log(ERROR, "Render System : createFramebuffer", "DirectionalShadowBlurring Framebuffer incomplete!");
		}
	}

	clearPrimaryBlurringFramebuffers();

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

void RenderSystem::blurPass(unsigned int main_color_texture, unsigned int ssao_texture, unsigned int amount)
{
	glDisable(GL_DEPTH_TEST);

	bloomSSAO_blur_Shader->use();
	first_iteration = true;

	horizontal = true;

	for (unsigned int i = 0; i < amount; i++)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, PrimaryBlurringFramebuffers[horizontal]);
		glDrawBuffers(2, &PrimaryBlurringFramebuffers_attachments[0]);

		bloomSSAO_blur_Shader->setBool("horizontal", horizontal);

		glViewport(0, 0, framebuffer_width / 2, framebuffer_height / 2);

		glActiveTexture(GL_TEXTURE0);
		bloomSSAO_blur_Shader->setInt("inputTexture_1", 0);
		glBindTexture(GL_TEXTURE_2D, first_iteration ? main_color_texture : PrimaryBlurringFramebuffer_bloom_textures[!horizontal]);

		glActiveTexture(GL_TEXTURE1);
		bloomSSAO_blur_Shader->setInt("inputTexture_2", 1);
		glBindTexture(GL_TEXTURE_2D, first_iteration ? ssao_texture : PrimaryBlurringFramebuffer_ssao_textures[!horizontal]);

		glBindVertexArray(quadVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindVertexArray(0);

		horizontal = !horizontal;

		if (first_iteration)
			first_iteration = false;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderSystem::directionalSoftShadowPass()
{
	glDisable(GL_DEPTH_TEST);

	unsigned int amount = 5;
	directional_shadow_blur_Shader->use();
	first_iteration = true;

	horizontal = true;

	for (unsigned int i = 0; i < amount; i++)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, DirectionalShadowBlurringFramebuffers[horizontal]);

		directional_shadow_blur_Shader->setBool("horizontal", horizontal);

		glViewport(0, 0, shadow_map_width * 2, 2 * shadow_map_height);

		glActiveTexture(GL_TEXTURE0);
		directional_shadow_blur_Shader->setInt("inputTexture_1", 0);
		glBindTexture(GL_TEXTURE_2D, first_iteration ? directional_shadow_framebuffer_depth_texture : DirectionalShadowBlurring_soft_shadow_textures[!horizontal]);

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

void RenderSystem::SSAOPass()
{
	glBindFramebuffer(GL_FRAMEBUFFER, SSAOFrameBuffer);
	glClear(GL_COLOR_BUFFER_BIT);
	glViewport(0, 0, framebuffer_width / 2, framebuffer_height / 2);

	SSAOShader->use();

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, DeferredGbuffer_position);
	SSAOShader->setInt("position_texture", 0);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, DeferredGbuffer_texture_normal_view); // Tangent space normals
	SSAOShader->setInt("normal_texture", 1);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, random_rotation_texture);
	SSAOShader->setInt("noise_texture", 2);

	std::stringstream ss;
	for (unsigned int i = 0; i < 8; i++)
	{
		ss.str("");
		ss << "[" << i << "]";
		SSAOShader->setVec3("kernel" + ss.str(), ssao_kernel[i]);
	}

	SSAOShader->setMat4("view", *camera_view_matrix);
	SSAOShader->setMat4("projection", *camera_projection_matrix);

	glBindVertexArray(quadVAO);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderSystem::createSSAOData()
{
	createSSAOKernel(8);
	createSSAONoise();

	glGenTextures(1, &random_rotation_texture);
	glBindTexture(GL_TEXTURE_2D, random_rotation_texture);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 4, 4, 0, GL_RGB, GL_FLOAT, &ssao_noise[0]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);


	glGenFramebuffers(1, &SSAOFrameBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, SSAOFrameBuffer);

	glGenTextures(1, &SSAOFramebuffer_color);
	glBindTexture(GL_TEXTURE_2D, SSAOFramebuffer_color);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, framebuffer_width / 2, framebuffer_height / 2, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, SSAOFramebuffer_color, 0);

	glBindTexture(GL_TEXTURE_2D, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderSystem::createSSAOKernel(unsigned int num_samples)
{
	std::uniform_real_distribution<float> randomFloats(0.1f, 1.0f);
	std::default_random_engine generator;

	for (unsigned i = 0; i < num_samples; ++i)
	{
		glm::vec3 sample(randomFloats(generator) * 2.0 - 1.0,
			randomFloats(generator) * 2.0 - 1.0,
			randomFloats(generator));

		sample = glm::normalize(sample);
		sample *= randomFloats(generator);

		float scale = (float)i / num_samples;
		scale = lerp(0.1f, 1.0f, scale * scale);
		ssao_kernel.push_back(sample);
	}
}

void RenderSystem::createSSAONoise()
{
	std::uniform_real_distribution<float> randomFloats(0.1f, 1.0f);
	std::default_random_engine generator;

	for (unsigned int i = 0; i < 16; ++i)
	{
		glm::vec3 noise(randomFloats(generator) * 2.0 - 1.0,
			randomFloats(generator) * 2.0 - 1.0,
			0.0);

		ssao_noise.push_back(noise);
	}
}

float RenderSystem::lerp(float a, float b, float t)
{
	return a + t * (b - a);
}