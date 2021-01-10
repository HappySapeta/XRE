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
#include <stb_image.h>


#include <logger.h>
#include <shader.h>
#include <lights.h>
#include <mesh.h>
#include <LightingProbes.h>
#include <CullingTester.h>
#include <LightingProbes.h>
#include <xre_configuration.h>

using namespace xre;

const char* GLErrorToString(GLenum err);

static LogModule* LOGGER = LogModule::getLoggerInstance();
ProbeRenderer* probeRenderer;
Renderer* Renderer::instance = NULL;

Renderer* Renderer::renderer()
{
	if (instance == NULL)
	{
		LOGGER->log(ERROR, "Render System : renderer", "No renderer instance available. Create one first!");
	}
	return instance;
}

// SHOULD BE CALLED ONLY ONCE, BEFORE ENTERING RENDER LOOP.
Renderer* Renderer::renderer(unsigned int screen_width, unsigned int screen_height, const glm::vec4& background_color, float lights_near_plane_p, float lights_far_plane_p, int shadow_map_width_p, int shadow_map_height_p, RENDER_PIPELINE render_pipeline, LIGHTING_MODE light_mode)
{
	if (instance == NULL)
	{
		instance = new Renderer(screen_width, screen_height, background_color, lights_near_plane_p, lights_far_plane_p, shadow_map_width_p, shadow_map_height_p, render_pipeline, light_mode);
	}
	return instance;
}

Renderer::Renderer(unsigned int screen_width, unsigned int screen_height, const glm::vec4& background_color, float lights_near_plane_p, float lights_far_plane_p, int shadow_map_width_p, int shadow_map_height_p, RENDER_PIPELINE render_pipeline, LIGHTING_MODE light_mode)
	:framebuffer_width(screen_width), framebuffer_height(screen_height), bg_color(background_color), light_near_plane(lights_near_plane_p), light_far_plane(lights_far_plane_p), shadow_map_width(shadow_map_height_p), shadow_map_height(shadow_map_height_p)
{
	positive_exponent = 20.0f;
	negative_exponent = 80.0f;
	num_draw_buffers = lighting_model == LIGHTING_MODE::PBR ? 4 : 3;


	rendering_pipeline = render_pipeline;
	lighting_model = light_mode;
	bg_color = background_color;

	probeRenderer = new ProbeRenderer(16, 128, 1024);

	if (rendering_pipeline == RENDER_PIPELINE::DEFERRED)
	{
		createDeferredBuffers();
		createShadowMapFramebuffers();


		if (lighting_model == LIGHTING_MODE::BLINNPHONG)
		{
			deferredFillShader = new Shader(
				"./Source/Resources/Shaders/DeferredAdditional/deferred_fill_vertex_shader.vert",
				"./Source/Resources/Shaders/DeferredAdditional/deferred_fill_bphong_fragment_shader.frag");

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

			glGenTextures(1, &brdfLUT);

			int texture_width, texture_height, num_channels;
			const char* filepath = "./Source/Resources/Textures/ibl_brdf_lut.png";
			unsigned char* data = stbi_load(filepath, &texture_width, &texture_height, &num_channels, 0);
			if (data)
			{
				LOGGER->log(INFO, "LOAD TEXTURE", "Loading texture : " + std::string(filepath));

				glBindTexture(GL_TEXTURE_2D, brdfLUT);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB, texture_width, texture_height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			}
			else
			{
				LOGGER->log(xre::ERROR, "LOAD TEXTURE", "Failed to create texture : " + std::string(filepath) + " : " + stbi_failure_reason());
			}
			stbi_image_free(data);
		}
	}
	else if (rendering_pipeline == RENDER_PIPELINE::FORWARD)
	{
		createForwardFramebuffers();
		createShadowMapFramebuffers();
	}


#pragma region Shader Initialization

	quadShader = new Shader
	(
		"./Source/Resources/Shaders/Quad/quad_vertex_shader.vert",
		"./Source/Resources/Shaders/Quad/quad_fragment_shader.frag"
	);

	depthShader_point = new Shader
	(
		"./Source/Resources/Shaders/ShadowMapping/depth_map_vertex_shader.vert",
		"./Source/Resources/Shaders/ShadowMapping/depth_map_point_fragment_shader.frag",
		"./Source/Resources/Shaders/ShadowMapping/depth_map_geometry_shader.geom");

	depthShader_directional = new Shader
	(
		"./Source/Resources/Shaders/ShadowMapping/depth_map_vertex_shader.vert",
		"./Source/Resources/Shaders/ShadowMapping/depth_map_directional_fragment_shader.frag",
		"./Source/Resources/Shaders/ShadowMapping/depth_map_geometry_shader.geom");

	bloomSSAO_blur_Shader = new Shader
	(
		"./Source/Resources/Shaders/Quad/quad_vertex_shader.vert",
		"./Source/Resources/Shaders/Blur/bloom_ssao_blur_shader.frag"
	);

	directional_shadow_blur_Shader = new Shader
	(
		"./Source/Resources/Shaders/Quad/quad_vertex_shader.vert",
		"./Source/Resources/Shaders/Blur/directional_soft_shadow_shadow.frag"
	);

	SSAOShader = new Shader(
		"./Source/Resources/Shaders/SSAO/ssao_vertex_shader.vert",
		"./Source/Resources/Shaders/SSAO/ssao_fragment_shader.frag"
	);

#pragma endregion

	createQuad();
	createSSAOData();
	createBlurringFramebuffers();

	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

	draw_queue.reserve(50);
	first_draw = true;
	directional_light = NULL;
}

void Renderer::sortDrawQueue()
{
	std::sort(draw_queue.begin(), draw_queue.end(), [=](model_information a, model_information b)
		{
			glm::vec3 a_centroid = (a.mesh_aabb.max_v + a.mesh_aabb.min_v) / glm::vec3(2.0);
			glm::vec3 b_centroid = (b.mesh_aabb.max_v + b.mesh_aabb.min_v) / glm::vec3(2.0);

			float d_a = glm::length(a_centroid - *camera_position);
			float d_b = glm::length(b_centroid - *camera_position);

			return d_a < d_b;
		});
}

void Renderer::Render()
{
	if (rendering_pipeline == RENDER_PIPELINE::DEFERRED)
	{
		std::thread frustum_test_thread(UpdateFrustumTestResults, *camera_view_matrix, *camera_projection_matrix, &draw_queue);
		std::thread sorting_thread(&Renderer::sortDrawQueue, this);

		frustum_test_thread.detach();
		sorting_thread.detach();

		if (point_lights.size() > 0 && shadow_frames % 5 == 0)
		{
			clearPointShadowFramebuffer();
			pointShadowPass();
		}

		if (directional_light && shadow_frames % 5 == 0)
		{
			clearDirectionalShadowMapFramebuffer();
			directionalShadowPass();
		}
		shadow_frames++;

		clearDeferredBuffers();
		deferredFillPass();
		//SSAOPass();
		deferredColorShader->use();
		deferredColorShader->setInt("use_ssao", 2);
		deferredColorPass();

		clearPrimaryBlurringFramebuffers();
		blurPass(DeferredFinal_secondary_texture, SSAOFramebuffer_color, 2); // blooooom....

		SoftShadowPass(2);

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

		frustum_test_thread.~thread();
		sorting_thread.~thread();

		if (!lightmaps_drawn)
		{
			lightmaps_drawn = true;
			probeRenderer->GenerateLightProbes(glm::vec3(16, 4, 6), glm::vec3(1, -3.5, -0.5), glm::vec3(0.2, 0.2, 0.25));
			probeRenderer->RenderProbes(
				&draw_queue, &point_lights,
				directional_light, &directional_light_space_matrix,
				&point_shadow_depth_storage[0],
				directional_shadow_depth_storage);
			probeRenderer->SetShaderAttributes(deferredColorShader);

			diffuse_irradiance_light_probe_cubemap_array = probeRenderer->light_probe_diffuse_irradiance_cubemap_array;
			specular_irradiance_light_probe_cubemap_array = probeRenderer->light_probe_specular_irradiance_cubemap_array;

			delete probeRenderer;
		}
	}
	else
	{
		std::thread frustum_test_thread(UpdateFrustumTestResults, *camera_view_matrix, *camera_projection_matrix, &draw_queue);
		std::thread sorting_thread(&Renderer::sortDrawQueue, this);

		frustum_test_thread.detach();
		sorting_thread.detach();

		if (point_lights.size() > 0 && shadow_frames % 10 == 0)
		{
			clearPointShadowFramebuffer();
			pointShadowPass();
		}

		if (directional_light && shadow_frames % 10 == 0)
		{
			clearDirectionalShadowMapFramebuffer();
			directionalShadowPass();
		}
		shadow_frames++;

		clearForwardFramebuffer();
		ForwardColorPass();

		clearPrimaryBlurringFramebuffers();
		blurPass(ForwardFramebuffer_secondary_texture, ForwardFramebuffer_secondary_texture, 5); // blooooom....

		SoftShadowPass(2);

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

		frustum_test_thread.~thread();
		sorting_thread.~thread();
	}
}

void Renderer::pushToDrawQueue(unsigned int vertex_array_object, unsigned int indices_size,
	const xre::Shader& object_shader, const glm::mat4& model_matrix,
	std::vector<Texture>* object_textures, std::vector<std::string>* texture_types,
	std::string model_name, bool is_dynamic,
	bool* setup_success, BoundingVolume aabb)


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
	model_info_i.mesh_aabb = aabb;
	model_info_i.frustum_cull = false;

	*setup_success = true;

	draw_queue.push_back(model_info_i);
}

void Renderer::createShadowMapFramebuffers()
{
#pragma region Directional Shadow Map
	glGenFramebuffers(1, &directional_shadow_framebuffer);
	glGenTextures(1, &directional_shadow_depth_storage);

	glBindTexture(GL_TEXTURE_2D, directional_shadow_depth_storage);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, shadow_map_width * 4, 4 * shadow_map_height, 0, GL_RGBA, GL_FLOAT, NULL);

	float border_color[] = { 0.0f,0.0f,0.0f,1.0f };

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_color);

	glBindFramebuffer(GL_FRAMEBUFFER, directional_shadow_framebuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, directional_shadow_depth_storage, 0);

	glGenRenderbuffers(1, &directional_shadow_renderbuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, directional_shadow_renderbuffer);

	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, shadow_map_width * 4, 4 * shadow_map_height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, directional_shadow_renderbuffer);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		LOGGER->log(ERROR, "Renderer : createShadowMapBuffer", "Directional Shadow Framebuffer is incomplete!");

	glBindTexture(GL_TEXTURE_2D, 0);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);


#pragma endregion

#pragma region Point Shadow Map

	glGenFramebuffers(XRE_MAX_POINT_SHADOW_MAPS, &point_shadow_framebuffer[0]);
	glGenTextures(XRE_MAX_POINT_SHADOW_MAPS, &point_shadow_depth_storage[0]);
	glGenTextures(XRE_MAX_POINT_SHADOW_MAPS, &point_shadow_depth_attachment[0]);

	for (unsigned int i = 0; i < XRE_MAX_POINT_SHADOW_MAPS; i++)
	{
		glBindTexture(GL_TEXTURE_CUBE_MAP, point_shadow_depth_storage[i]);
		for (unsigned int j = 0; j < 6; j++)
		{
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + j, 0, GL_RGBA16F, shadow_map_width, shadow_map_height, 0, GL_RGBA, GL_FLOAT, NULL);
		}

		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		glBindFramebuffer(GL_FRAMEBUFFER, point_shadow_framebuffer[i]);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, point_shadow_depth_storage[i], 0);

		glBindTexture(GL_TEXTURE_CUBE_MAP, point_shadow_depth_attachment[i]);
		for (int j = 0; j < 6; j++)
		{
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + j, 0, GL_DEPTH_COMPONENT, shadow_map_width, shadow_map_height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
		}

		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		glBindFramebuffer(GL_FRAMEBUFFER, point_shadow_framebuffer[i]);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, point_shadow_depth_attachment[i], 0);

		glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	}
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		LOGGER->log(ERROR, "Renderer :  createFrameBuffers ", "Point Shadow Static Framebuffer is incomplete!");

#pragma endregion
}

void Renderer::createDeferredBuffers()
{
	// Main Color Buffer

	glGenFramebuffers(1, &DeferredFinalBuffer);

	glBindFramebuffer(GL_FRAMEBUFFER, DeferredFinalBuffer);

	glGenTextures(1, &DeferredFinal_primary_texture);
	glBindTexture(GL_TEXTURE_2D, DeferredFinal_primary_texture);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, framebuffer_width, framebuffer_height, 0, GL_RGB, GL_FLOAT, NULL);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, DeferredFinal_primary_texture, 0);

	// ----------------------------------

	glGenTextures(1, &DeferredFinal_secondary_texture);
	glBindTexture(GL_TEXTURE_2D, DeferredFinal_secondary_texture);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, framebuffer_width, framebuffer_height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

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

	// Albedo Buffer (with alpha)
	glGenTextures(1, &DeferredGbuffer_color);
	glBindTexture(GL_TEXTURE_2D, DeferredGbuffer_color);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, framebuffer_width, framebuffer_height, 0, GL_RGBA, GL_FLOAT, NULL);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, DeferredGbuffer_color, 0);
	// ----------------------------------

	// Normal Buffer
	glGenTextures(1, &DeferredGbuffer_normal);
	glBindTexture(GL_TEXTURE_2D, DeferredGbuffer_normal);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, framebuffer_width, framebuffer_height, 0, GL_RGB, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, DeferredGbuffer_normal, 0);
	// ----------------------------------

	// Depth Buffer
	glGenTextures(1, &DeferredGbuffer_depth);
	glBindTexture(GL_TEXTURE_2D, DeferredGbuffer_depth);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, framebuffer_width, framebuffer_height, 0, GL_RED, GL_FLOAT, NULL);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, DeferredGbuffer_depth, 0);
	// ----------------------------------

	// MOR Buffer - Metalness, Occlusion, Roughness
	if (lighting_model == LIGHTING_MODE::PBR)
	{
		glGenTextures(1, &DeferredGbuffer_texture_mor);
		glBindTexture(GL_TEXTURE_2D, DeferredGbuffer_texture_mor);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_R3_G3_B2, framebuffer_width, framebuffer_height, 0, GL_RGB, GL_FLOAT, NULL);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glBindTexture(GL_TEXTURE_2D, 0);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, DeferredGbuffer_texture_mor, 0);
	}

	// ----------------------------------

	// ---------------------------------------------------------------------------------------------------------------------

	glGenRenderbuffers(1, &DeferredRenderBuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, DeferredRenderBuffer);

	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, framebuffer_width, framebuffer_height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, DeferredRenderBuffer);

	glBindRenderbuffer(GL_RENDERBUFFER, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		LOGGER->log(ERROR, "Renderer : GBuffer", "GBuffer is incomplete!");

	for (unsigned int i = 0; i < num_draw_buffers; i++)
	{
		DeferredFrameBuffer_primary_color_attachments[i] = GL_COLOR_ATTACHMENT0 + i;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

void Renderer::clearDeferredBuffers()
{
	glBindFramebuffer(GL_FRAMEBUFFER, DeferredDataFrameBuffer);
	glClearColor(bg_color.x, bg_color.y, bg_color.z, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glBindFramebuffer(GL_FRAMEBUFFER, DeferredFinalBuffer);
	glClear(GL_COLOR_BUFFER_BIT);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::deferredFillPass()
{
	glBindFramebuffer(GL_FRAMEBUFFER, DeferredDataFrameBuffer);
	glDrawBuffers(num_draw_buffers, &DeferredFrameBuffer_primary_color_attachments[0]);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glViewport(0, 0, framebuffer_width, framebuffer_height);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	deferredFillShader->use();
	deferredFillShader->setMat4("view", *camera_view_matrix);
	deferredFillShader->setMat4("projection", *camera_projection_matrix);
	deferredFillShader->setVec3("camera_position", *camera_position);


	for (unsigned int i = 0; i < draw_queue.size(); i++)
	{
		if (draw_queue[i].frustum_cull == true)
		{
			continue;
		}

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

void Renderer::deferredColorPass()
{
	glBindFramebuffer(GL_FRAMEBUFFER, DeferredFinalBuffer);
	glDrawBuffers(2, &DeferredFinal_attachments[0]);

	glDisable(GL_DEPTH_TEST);
	glViewport(0, 0, framebuffer_width, framebuffer_height);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	deferredColorShader->setMat4("view", *camera_view_matrix);
	deferredColorShader->setVec3("camera_pos", *camera_position);
	deferredColorShader->setVec3("camera_look_direction", *camera_front);
	deferredColorShader->setFloat("near", light_near_plane);
	deferredColorShader->setFloat("far", light_far_plane);
	deferredColorShader->setBool("directional_lighting_enabled", directional_light != NULL);
	deferredColorShader->setInt("N_POINT", point_lights.size());
	deferredColorShader->setMat4("inv_projection", glm::inverse(*camera_projection_matrix));
	deferredColorShader->setMat4("inv_view", glm::inverse(*camera_view_matrix));
	deferredColorShader->setFloat("positive_exponent", positive_exponent);
	deferredColorShader->setFloat("negative_exponent", negative_exponent);

	glActiveTexture(GL_TEXTURE0);
	deferredColorShader->setInt("diffuse_texture", 0);
	glBindTexture(GL_TEXTURE_2D, DeferredGbuffer_color);

	glActiveTexture(GL_TEXTURE1);
	deferredColorShader->setInt("normal_texture", 1);
	glBindTexture(GL_TEXTURE_2D, DeferredGbuffer_normal);

	glActiveTexture(GL_TEXTURE2);
	deferredColorShader->setInt("depth_texture", 2);
	glBindTexture(GL_TEXTURE_2D, DeferredGbuffer_depth);

	glActiveTexture(GL_TEXTURE3);
	deferredColorShader->setInt("ssao_texture", 3);
	glBindTexture(GL_TEXTURE_2D, PrimaryBlurringFramebuffer_ssao_textures[0]);

	glActiveTexture(GL_TEXTURE4);
	deferredColorShader->setInt("mor_texture", 4);
	glBindTexture(GL_TEXTURE_2D, DeferredGbuffer_texture_mor);

	glActiveTexture(GL_TEXTURE5);
	deferredColorShader->setInt("diffuse_irradiance_light_probe_cubemaps", 5);
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, diffuse_irradiance_light_probe_cubemap_array);

	glActiveTexture(GL_TEXTURE6);
	deferredColorShader->setInt("specular_irradiance_light_probe_cubemaps", 6);
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, specular_irradiance_light_probe_cubemap_array);

	glActiveTexture(GL_TEXTURE7);
	deferredColorShader->setInt("brdfLUT", 7);
	glBindTexture(GL_TEXTURE_2D, brdfLUT);

	if (directional_light != NULL)
	{
		glActiveTexture(GL_TEXTURE8);
		glBindTexture(GL_TEXTURE_2D, DirectionalShadowBlurring_soft_shadow_textures[0]);
		deferredColorShader->setInt("directional_shadow_depth_map", 8);

		deferredColorShader->setMat4("directional_light_space_matrix", directional_light_space_matrix);
	}

	std::stringstream ss;
	for (unsigned int i = 0; i < XRE_MAX_POINT_SHADOW_MAPS; i++)
	{
		ss.str("");
		ss << '[' << i << ']';

		glActiveTexture(GL_TEXTURE9 + i);
		glBindTexture(GL_TEXTURE_CUBE_MAP, point_shadow_depth_storage[i]);
		deferredColorShader->setInt("point_shadow_depth_map" + ss.str(), 9 + i);
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

void Renderer::directionalShadowPass()
{
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	depthShader_directional->use();
	depthShader_directional->setInt("is_point", 0);

	if (directional_light != NULL)
	{
		glViewport(0, 0, shadow_map_width * 4, 4 * shadow_map_height);
		glBindFramebuffer(GL_FRAMEBUFFER, directional_shadow_framebuffer);
		depthShader_directional->setInt("faces", 1);

		createDirectionalLightMatrix(directional_light->m_position, directional_light->m_direction);

		depthShader_directional->setMat4("light_space_matrix_cube[0]", directional_light_space_matrix);
		depthShader_directional->setVec3("lightPos", directional_light->m_position);
		depthShader_directional->setFloat("farPlane", light_far_plane);
		depthShader_directional->setMat4("directional_light_space_matrix", directional_light_space_matrix);
		depthShader_directional->setFloat("positive_exponent", positive_exponent);
		depthShader_directional->setFloat("negative_exponent", negative_exponent);

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

void Renderer::pointShadowPass()
{
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);
	glViewport(0, 0, shadow_map_width, shadow_map_height);

	depthShader_point->use();
	depthShader_point->setInt("faces", 6);
	depthShader_point->setInt("is_point", 1);

	for (unsigned int k = 0; k < XRE_MAX_POINT_SHADOW_MAPS; k++)
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

		glViewport(0, 0, shadow_map_width, shadow_map_height);
		glBindFramebuffer(GL_FRAMEBUFFER, point_shadow_framebuffer[k]);
		glClear(GL_DEPTH_BUFFER_BIT);

		depthShader_point->setInt("mode", 0); // 0 for static
		glColorMask(true, true, false, false);
		if (first_draw)
		{
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

		depthShader_point->setInt("mode", 1); // 1 for dynamic
		glColorMask(false, false, true, true);

		for (unsigned int i = 0; i < draw_queue.size(); i++)
		{
			if (!draw_queue[i].dynamic)
			{
				continue;
			}

			glm::vec3 object_bb_position = (draw_queue[i].mesh_aabb.max_v + draw_queue[i].mesh_aabb.min_v) / glm::vec3(2.0);

			if (glm::length(object_bb_position - point_lights[k]->m_position) > 3)
				continue;

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
	glColorMask(true, true, true, true);
}

void Renderer::createForwardFramebuffers()
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

	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, framebuffer_width, framebuffer_height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, ForwardFramebufferRenderbuffer);

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

void Renderer::ForwardColorPass() // draw everything to Framebuffer. 2nd pass.
{
	glEnable(GL_DEPTH_TEST);
	glViewport(0, 0, framebuffer_width, framebuffer_height);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	glBindFramebuffer(GL_FRAMEBUFFER, ForwardFramebuffer);
	glDrawBuffers(2, &ForwardFramebuffer_Color_Attachments[0]);

	std::stringstream ss;

	for (unsigned int i = 0; i < draw_queue.size(); i++) //optimize state changes.
	{
		if (draw_queue[i].frustum_cull == true)
		{
			continue;
		}

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

		if (point_lights.size() > 0)
		{
			for (unsigned int k = j; k < XRE_MAX_POINT_SHADOW_MAPS + j; k++)
			{
				ss.str("");
				ss << "[" << k - j + 1 << "]";

				draw_queue[i].object_shader->setVec3("light_position_vertex" + ss.str(), point_lights[k - j]->m_position);

				ss.str("");
				ss << "[" << k - j << "]";
				glActiveTexture(GL_TEXTURE0 + k);
				draw_queue[i].object_shader->setInt("point_shadow_depth_map" + ss.str(), k);
				glBindTexture(GL_TEXTURE_CUBE_MAP, point_shadow_depth_storage[k - j]);
			}
		}

		j += XRE_MAX_POINT_SHADOW_MAPS;

		if (directional_light != NULL)
		{
			draw_queue[i].object_shader->setMat4("directional_light_space_matrix", directional_light_space_matrix);
			draw_queue[i].object_shader->setVec3("directional_light_position", directional_light->m_position);

			glActiveTexture(GL_TEXTURE0 + j);
			draw_queue[i].object_shader->setInt("directional_shadow_depth_map", j);
			glBindTexture(GL_TEXTURE_2D, DirectionalShadowBlurring_soft_shadow_textures[0]);

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

void Renderer::clearForwardFramebuffer()
{
	glBindFramebuffer(GL_FRAMEBUFFER, ForwardFramebuffer);

	glClearColor(bg_color.x, bg_color.y, bg_color.z, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::clearDefaultFramebuffer()
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClearColor(bg_color.x, bg_color.y, bg_color.z, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
}

void Renderer::clearDirectionalShadowMapFramebuffer()
{
	glBindFramebuffer(GL_FRAMEBUFFER, directional_shadow_framebuffer);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::clearDirectionalShadowBlurringFramebuffers()
{
	for (unsigned int i = 0; i < 2; i++)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, DirectionalShadowBlurringFramebuffers[i]);
		glClearColor(1.0, 1.0, 1.0, 1.0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}
}

void Renderer::clearPointShadowFramebuffer()
{
	glColorMask(false, false, true, true);
	for (unsigned int i = 0; i < XRE_MAX_POINT_SHADOW_MAPS; i++)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, point_shadow_framebuffer[i]);
		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glColorMask(true, true, true, true);

}

void Renderer::clearPrimaryBlurringFramebuffers()
{
	glClearColor(bg_color.r, bg_color.g, bg_color.b, 1.0);
	for (unsigned int i = 0; i < 2; i++)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, PrimaryBlurringFramebuffers[i]);
		glClear(GL_COLOR_BUFFER_BIT);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::createDirectionalLightMatrix(glm::vec3 light_position, glm::vec3 light_front)
{
	directional_light_projection = glm::ortho(-15.0f, 15.0f, -15.0f, 15.0f, light_near_plane, light_far_plane);
	directional_light_space_matrix = directional_light_projection * glm::lookAt(light_position, glm::vec3(0.0f), glm::cross(xre::WORLD_RIGHT, light_front));
}

void Renderer::createPointLightMatrices(glm::vec3 light_position, unsigned int light_index)
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

void Renderer::createBlurringFramebuffers()
{
	// Primary Blurring Framebuffers
	glGenFramebuffers(2, PrimaryBlurringFramebuffers);
	glGenTextures(2, &PrimaryBlurringFramebuffer_bloom_textures[0]);
	glGenTextures(2, &PrimaryBlurringFramebuffer_ssao_textures[0]);

	for (unsigned int i = 0; i < 2; i++)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, PrimaryBlurringFramebuffers[i]);

		glBindTexture(GL_TEXTURE_2D, PrimaryBlurringFramebuffer_bloom_textures[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, framebuffer_width / 4, framebuffer_height / 4, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, PrimaryBlurringFramebuffer_bloom_textures[i], 0);

		glBindTexture(GL_TEXTURE_2D, PrimaryBlurringFramebuffer_ssao_textures[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RG, framebuffer_width / 4, framebuffer_height / 4, 0, GL_RG, GL_UNSIGNED_BYTE, NULL);
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

	// Shadow Blurring Framebuffers
	glGenFramebuffers(2, DirectionalShadowBlurringFramebuffers);
	glGenTextures(2, &DirectionalShadowBlurring_soft_shadow_textures[0]);

	for (unsigned int i = 0; i < 2; i++)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, DirectionalShadowBlurringFramebuffers[i]);

		glBindTexture(GL_TEXTURE_2D, DirectionalShadowBlurring_soft_shadow_textures[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, shadow_map_width * 4, 4 * shadow_map_height, 0, GL_RED, GL_FLOAT, NULL);
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

void Renderer::addToLights(Light* light)
{
	if (typeid(*light) == typeid(DirectionalLight))
	{
		if (directional_light == NULL)
		{
			directional_light = (DirectionalLight*)light;
		}
		else
		{
			LOGGER->log(ERROR, " Renderer::addToRenderer(Light* light)", "Cannot have more than 1 directional light.");
		}
	}
	else
	{
		point_lights.push_back((PointLight*)light);
	}
	lights.push_back(light);
}

void Renderer::setCameraMatrices(const glm::mat4* view, const glm::mat4* projection, const glm::vec3* position, const glm::vec3* front)
{
	camera_view_matrix = view;
	camera_projection_matrix = projection;
	camera_position = position;
	camera_front = front;
}

void Renderer::blurPass(unsigned int main_color_texture, unsigned int ssao_texture, unsigned int amount)
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

		glViewport(0, 0, framebuffer_width / 4, framebuffer_height / 4);

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

void Renderer::SoftShadowPass(unsigned int amount)
{
	if (directional_light)
	{
		glDisable(GL_DEPTH_TEST);
		glViewport(0, 0, shadow_map_width * 4, 4 * shadow_map_height);

		directional_shadow_blur_Shader->use();
		first_iteration = true;

		horizontal = true;

		for (unsigned int i = 0; i < amount; i++)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, DirectionalShadowBlurringFramebuffers[horizontal]);

			directional_shadow_blur_Shader->setBool("horizontal", horizontal);

			glActiveTexture(GL_TEXTURE0);
			directional_shadow_blur_Shader->setInt("inputTexture_1", 0);
			glBindTexture(GL_TEXTURE_2D, first_iteration ? directional_shadow_depth_storage : DirectionalShadowBlurring_soft_shadow_textures[!horizontal]);

			glBindVertexArray(quadVAO);
			glDrawArrays(GL_TRIANGLES, 0, 6);
			glBindVertexArray(0);

			horizontal = !horizontal;

			if (first_iteration)
				first_iteration = false;
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
}

void Renderer::createQuad()
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

void Renderer::SSAOPass()
{
	glBindFramebuffer(GL_FRAMEBUFFER, SSAOFrameBuffer);
	glClear(GL_COLOR_BUFFER_BIT);
	glViewport(0, 0, framebuffer_width, framebuffer_height);

	SSAOShader->use();

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, DeferredGbuffer_depth);
	SSAOShader->setInt("depth_texture", 0);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, DeferredGbuffer_normal);
	SSAOShader->setInt("normal_texture", 1);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, random_rotation_texture);
	SSAOShader->setInt("noise_texture", 2);

	std::stringstream ss;
	for (unsigned int i = 0; i < ssao_kernel.size(); i++)
	{
		ss.str("");
		ss << "[" << i << "]";
		SSAOShader->setVec3("kernel" + ss.str(), ssao_kernel[i]);
	}

	SSAOShader->setMat4("inv_projection", glm::inverse(*camera_projection_matrix));
	SSAOShader->setMat4("projection", *camera_projection_matrix);
	SSAOShader->setMat4("view", *camera_view_matrix);

	glBindVertexArray(quadVAO);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::createSSAOData()
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

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, framebuffer_width, framebuffer_height, 0, GL_RED, GL_FLOAT, NULL);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, SSAOFramebuffer_color, 0);

	glBindTexture(GL_TEXTURE_2D, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::createSSAOKernel(unsigned int num_samples)
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
		scale = 0.1f + (scale * scale) * 0.9f;
		ssao_kernel.push_back(sample);
	}
}

void Renderer::createSSAONoise()
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