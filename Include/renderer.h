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


namespace xre
{
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

		RenderSystem(unsigned int screen_width, unsigned int screen_height, const glm::vec4& background_color, float lights_near_plane_p, float lights_far_plane_p, int shadow_map_width_p, int shadow_map_height_p);

		void createFramebuffers();
		void createQuad();
		void createShadowMapFramebuffer();
		void createDirectionalLightMatrix(glm::vec3 light_position, glm::vec3 light_front);
		void createPointLightMatrices(glm::vec3 light_position, unsigned int light_index);
		void createGBuffer();
		void clearRenderFramebuffer();
		void clearDefaultFramebuffer();
		void clearDirectionalShadowMapFramebuffer();
		void clearPointShadowFramebufferStatic();
		void clearPointShadowFramebufferDynamic();
		void clearPingPongBuffers();
		void directionalShadowPass();
		void pointShadowPass();
		void colorPass();
		void blurPass();

		static RenderSystem* instance;

		// just making shit more complicated than it has to be.
		unsigned int framebuffer_width, framebuffer_height;

		// Forward Shading
		unsigned int
			FBO,
			FBO_primary_texture,
			FBO_secondary_texture,
			FBO_Color_Attachments[2],
			FBO_depth_texture,
			RBO,
			PingPongFBO[2],
			PingPong_textures[2];

		// Deferred Shading
		unsigned int GBuffer;
		unsigned int GB_position, GB_albedo, GB_specular, GB_normal;
		unsigned int GB_attachments[4];



		bool horizontal = true, first_iteration = true;
		bool first_draw = true;

		unsigned int directional_shadow_framebuffer, directional_shadow_framebuffer_depth_texture; // A cubermap texture for a directional light to facilitate compatibility with the geometry shader.
		unsigned int point_shadow_framebuffer_static[5], point_shadow_framebuffer_dynamic[5],
					 point_shadow_framebuffer_depth_texture_cubemap_static[5],
					 point_shadow_framebuffer_depth_texture_cubemap_dynamic[5]; // I challenge you to pronounce the entire name in 16 ms.
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
		Shader* depthShader;
		Shader* blurShader;
		unsigned int screen_texture;

		//---------------------------------------------

		const glm::mat4* camera_view_matrix;
		const glm::mat4* camera_projection_matrix;
		const glm::vec3* camera_position;

		//---------------------------------------------


		glm::vec4 bg_color;

		class PostProcessing
		{
		public:
			static void updateBloom(bool* const enabled, float* const intensity, float* const threshold, float* const radius, glm::vec3* const color);
			static void updateBlur(bool* const enabled, float* const intensity, float* const radius, int* const kernel_size);
			static void updateSSAO(bool* const enabled, float* const intensity, float* const radius, glm::vec3* const color);
		private:
			struct bloom_options
			{
				bool enabled;
				float intensity;
				float threshold;
				float radius;
				glm::vec3 color;
			} bloom_options_i;

			struct blur_options
			{
				bool enabled;
				float intensity;
				float radius;
				int kernel_size;
			} blur_options_i;

			struct ssao_options
			{
				bool enabled;
				float intensity;
				float radius;
				glm::vec3 color;
			} ssao_options_i;
		};

	public:
		static RenderSystem* renderer();
		static RenderSystem* renderer(unsigned int screen_width, unsigned int screen_height, const glm::vec4& background_color, float lights_near_plane_p, float lights_far_plane_p, int shadow_map_width_p, int shadow_map_height_p);

		RenderSystem(RenderSystem& other) = delete;

		void draw(unsigned int vertex_array_object, unsigned int indices_size, const xre::Shader& object_shader, const glm::mat4& model_matrix, std::vector<Texture>* object_textures, std::vector<std::string>* texture_types, std::string model_name, bool isdynamic, bool* setup_success);
		void drawToScreen();
		void SwitchPfx(bool option);

		void setCameraMatrices(const glm::mat4* view, const glm::mat4* projection, const glm::vec3* position);
		void addToRenderSystem(Light* light);

		PostProcessing* postfx;

		glm::vec3 world_view_pos;
	};
}
#endif