#ifndef RENDERER_H
#define RENDERER_H

#include <glad/glad.h>
#include <glm/glm.hpp>


#include <mesh.h>
#include <shader.h>
#include <lights.h>
#include <xre_configuration.h>

#include <string>
#include <vector>
#include <random>

namespace xre
{
	enum RENDER_PIPELINE
	{
		DEFERRED,
		FORWARD
	};
	enum LIGHTING_MODE
	{
		PBR,
		BLINNPHONG
	};

	class RenderSystem
	{
	private:
		struct model_information
		{
			bool* setup_success = NULL;
			std::string model_name = "";
			bool dynamic;
			unsigned int object_VAO = 0;
			unsigned int indices_size = 0;
			const xre::Shader* object_shader = NULL;
			const glm::mat4* object_model_matrix = NULL;
			std::vector<Texture>* object_textures = NULL;
			std::vector<std::string>* texture_types = NULL;
		};

		struct point_light_space_matrix_cube
		{
			glm::mat4 point_light_space_matrix_0;
			glm::mat4 point_light_space_matrix_1;
			glm::mat4 point_light_space_matrix_2;
			glm::mat4 point_light_space_matrix_3;
			glm::mat4 point_light_space_matrix_4;
			glm::mat4 point_light_space_matrix_5;
		};

		RenderSystem(unsigned int screen_width, unsigned int screen_height, const glm::vec4& background_color, float lights_near_plane_p, float lights_far_plane_p, int shadow_map_width_p, int shadow_map_height_p, RENDER_PIPELINE render_pipeline, LIGHTING_MODE light_mode);

		void createForwardFramebuffers();
		void createQuad();
		void createShadowMapFramebuffers();
		void createDeferredBuffers();
		void clearDeferredBuffers();
		void createDirectionalLightMatrix(glm::vec3 light_position, glm::vec3 light_front);
		void createPointLightMatrices(glm::vec3 light_position, unsigned int light_index);
		void createBlurringFramebuffers();
		void clearForwardFramebuffer();
		void clearDefaultFramebuffer();
		void clearDirectionalShadowMapFramebuffer();
		void clearDirectionalShadowBlurringFramebuffers();
		void clearPointShadowFramebufferStatic();
		void clearPointShadowFramebufferDynamic();
		void clearPrimaryBlurringFramebuffers();
		void directionalShadowPass();
		void pointShadowPass();
		void ForwardColorPass();
		void deferredFillPass();
		void deferredColorPass();
		void blurPass(unsigned int main_color_texture, unsigned int ssao_texture, unsigned int amount);
		void directionalSoftShadowPass();
		void SSAOPass();
		void createSSAOData();
		void createSSAOKernel(unsigned int num_samples);
		void createSSAONoise();
		float lerp(float a, float b, float t);

		static RenderSystem* instance;

		RENDER_PIPELINE rendering_pipeline;
		LIGHTING_MODE lighting_model;

		// just making shit more complicated than it has to be.
		unsigned int framebuffer_width, framebuffer_height;

		// Forward Shading
		unsigned int
			ForwardFramebuffer,
			ForwardFramebuffer_primary_texture,
			ForwardFramebuffer_secondary_texture,
			ForwardFramebuffer_Color_Attachments[2],
			ForwardFramebuffer_depth_texture,
			ForwardFramebufferRenderbuffer;

		// Deferred Shading
		unsigned int DeferredDataFrameBuffer,
			DeferredFinalBuffer;

		unsigned int DeferredRenderBuffer;

		unsigned int DeferredFinal_primary_texture,
			DeferredFinal_secondary_texture;

		unsigned int DeferredGbuffer_position,
			DeferredGbuffer_color,
			DeferredGbuffer_model_normal,
			DeferredGbuffer_tangent,
			DeferredGbuffer_texture_normal,
			DeferredGbuffer_texture_normal_view,
			DeferredGbuffer_texture_mor;

		unsigned int DeferredFrameBuffer_primary_color_attachments[6];
		unsigned int DeferredFinal_attachments[2];

		// SSAO
		unsigned int SSAOFrameBuffer;
		unsigned int SSAOFramebuffer_color;

		std::vector<glm::vec3> ssao_kernel;
		std::vector<glm::vec3> ssao_noise;

		// PrimaryBlurring Buffers
		unsigned int PrimaryBlurringFramebuffers[2],
			PrimaryBlurringFramebuffer_bloom_textures[2],
			PrimaryBlurringFramebuffer_ssao_textures[2],
			PrimaryBlurringFramebuffers_attachments[2];

		// DirectionalShadowBlur Buffers
		unsigned int DirectionalShadowBlurringFramebuffers[2],
			DirectionalShadowBlurring_soft_shadow_textures[2];

		unsigned int random_rotation_texture;

		Shader* deferredFillShader;
		Shader* deferredColorShader;
		Shader* SSAOShader;


		bool horizontal = true, first_iteration = true;
		bool first_draw = true;

		unsigned int directional_shadow_framebuffer,
			directional_shadow_framebuffer_depth_texture, // A cubemap texture for a directional light to facilitate compatibility with the geometry shader.
			directional_shadow_framebuffer_renderbuffer;

		unsigned int point_shadow_framebuffer_static[5], point_shadow_framebuffer_dynamic[5],
			point_shadow_framebuffer_depth_texture_cubemap_static[5], point_shadow_framebuffer_depth_texture_cubemap_dynamic[5],
			point_shadow_framebuffer_depth_color_texture_static[5], point_shadow_framebuffer_depth_color_texture_dynamic[5];
		
		// Dual Paraboloid Shadow Mapping for point lights.
		unsigned int point_para_depth_color_static[5], point_para_depth_color_dynamic[5];

		unsigned int point_shadow_depth_attachment_static[5], point_shadow_depth_attachment_dynamic[5];

		unsigned int shadow_map_width, shadow_map_height;
		float light_near_plane, light_far_plane;

		DirectionalLight* directional_light;
		std::vector<PointLight*> point_lights;

		glm::mat4 directional_light_projection;
		glm::mat4 directional_light_space_matrix;
		glm::mat4 point_light_projection;
		point_light_space_matrix_cube point_light_space_matrix_cube_array[5];

		std::vector<Light*> lights;
		std::vector<model_information> draw_queue;

		unsigned int quadVAO, quadVBO;
		Shader* quadShader;
		Shader* depthShader_point;
		Shader* depthShader_directional;
		Shader* bloomSSAO_blur_Shader;
		Shader* directional_shadow_blur_Shader;
		unsigned int screen_texture;

		bool deferred;

		//---------------------------------------------

		const glm::mat4* camera_view_matrix;
		const glm::mat4* camera_projection_matrix;
		const glm::vec3* camera_position;

		//---------------------------------------------


		glm::vec4 bg_color;

	public:
		static RenderSystem* renderer();
		static RenderSystem* renderer(unsigned int screen_width, unsigned int screen_height, const glm::vec4& background_color, float lights_near_plane_p, float lights_far_plane_p, int shadow_map_width_p, int shadow_map_height_p, RENDER_PIPELINE render_pipeline, LIGHTING_MODE light_mode);

		RenderSystem(RenderSystem& other) = delete;

		void draw(unsigned int vertex_array_object, unsigned int indices_size, const xre::Shader& object_shader, const glm::mat4& model_matrix, std::vector<Texture>* object_textures, std::vector<std::string>* texture_types, std::string model_name, bool isdynamic, bool* setup_success);
		void drawToScreen();
		void SwitchPfx(bool option);

		void setCameraMatrices(const glm::mat4* view, const glm::mat4* projection, const glm::vec3* position);
		void addToRenderSystem(Light* light);

		glm::vec3 world_view_pos;
	};
}
#endif