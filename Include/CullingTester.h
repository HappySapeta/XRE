#ifndef CULLINGTESTER_H
#define CULLINGTESTER_H

#include <mesh.h>
#include <shader.h>
#include <renderer.h>
#include <thread>

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace xre
{
	bool planeTest(glm::vec4 coeff, glm::vec3 p)
	{
		coeff = coeff / glm::length(glm::vec3(coeff.x, coeff.y, coeff.z));
		return (coeff.x * p.x + coeff.y * p.y + coeff.z * p.z + coeff.w > 0);
	}

	void UpdateFrustumTestResults(glm::mat4 view, glm::mat4 projection, std::vector<model_information>* draw_queue)
	{

		for (unsigned int d = 0; d < draw_queue->size(); d++)
		{
			BoundingVolume aabb = draw_queue->at(d).mesh_aabb;
			glm::mat4 model = *draw_queue->at(d).object_model_matrix;

			glm::vec4 b_pos[8];

			b_pos[0] = glm::vec4(aabb.min_v, 1.0);
			b_pos[7] = glm::vec4(aabb.max_v, 1.0);
			b_pos[1] = glm::vec4(b_pos[7].x, b_pos[0].y, b_pos[0].z, 1.0);
			b_pos[2] = glm::vec4(b_pos[7].x, b_pos[7].y, b_pos[0].z, 1.0);
			b_pos[3] = glm::vec4(b_pos[0].x, b_pos[7].y, b_pos[0].z, 1.0);
			b_pos[4] = glm::vec4(b_pos[0].x, b_pos[7].y, b_pos[7].z, 1.0);
			b_pos[5] = glm::vec4(b_pos[0].x, b_pos[0].y, b_pos[7].z, 1.0);
			b_pos[6] = glm::vec4(b_pos[7].x, b_pos[0].y, b_pos[7].z, 1.0);

			glm::mat4 M = glm::transpose(projection);

			glm::vec4 coeff[6];
			coeff[0] = glm::vec4(M[3][0] + M[0][0], M[3][1] + M[0][1], M[3][2] + M[0][2], M[3][3] + M[0][3]); // Left Plane
			coeff[1] = glm::vec4(M[3][0] - M[0][0], M[3][1] - M[0][1], M[3][2] - M[0][2], M[3][3] - M[0][3]); // Right Plane
			coeff[2] = glm::vec4(M[3][0] + M[1][0], M[3][1] + M[1][1], M[3][2] + M[1][2], M[3][3] + M[1][3]); // Bottom Plane
			coeff[3] = glm::vec4(M[3][0] - M[1][0], M[3][1] - M[1][1], M[3][2] - M[1][2], M[3][3] - M[1][3]); // Top Plane
			coeff[4] = glm::vec4(M[3][0] + M[2][0], M[3][1] + M[2][1], M[3][2] + M[2][2], M[3][3] + M[2][3]); // Near Plane
			coeff[5] = glm::vec4(M[3][0] - M[2][0], M[3][1] - M[2][1], M[3][2] - M[2][2], M[3][3] - M[2][3]); // Far Plane

			int iTotalIn = 0;
			for (unsigned int i = 0; i < 6; i++)
			{
				int inCount = 8;
				int iPtIn = 1;

				for (unsigned int j = 0; j < 8; j++)
				{
					glm::vec4 p = view * model * b_pos[j];

					if (planeTest(coeff[i], p) == false)
					{
						iPtIn = 0;
						--inCount;
					}
				}

				if (inCount == 0)
				{
					draw_queue->at(d).frustum_cull = true;
					break;
				}

				iTotalIn += iPtIn;
			}

			if (iTotalIn > 0)
			{
				draw_queue->at(d).frustum_cull = false;
			}
		}
	}
}
#endif