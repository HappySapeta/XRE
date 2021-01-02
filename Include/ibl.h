#ifndef IBL_H
#define IBL_H

#include <string>

#include <shader.h>
#include <tuple>

namespace xre
{
	class IBL
	{
	private:

		struct LightProbe
		{
			unsigned int irradiance_cubemap;
			glm::vec3 position;
		};

		static void convertEquiToCube(unsigned int hdrTexture, unsigned int captureFBO, unsigned int envCubemap, unsigned int cubeVAO);
		static unsigned int createCube();
		static std::tuple<unsigned int, unsigned int> createRenderingData(unsigned int resolution);

		IBL();
	public:
		static void HDRIToCubemap(std::string texture_path, unsigned int* output_cubemap);
		static void RenderIrradiance(unsigned int input_cubemap_texture, unsigned int* output_cubemap);
	};
}

#endif