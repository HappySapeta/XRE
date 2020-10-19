#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

namespace xre
{
	struct CameraMatrix
	{
		glm::mat4 projection;
		glm::mat4 view;
	};

	class Camera
	{
	private:
		glm::mat4 m_view = glm::mat4(1.0f);
		glm::mat4 m_projection = glm::mat4(1.0f);
		GLFWwindow* m_window = NULL;
		bool m_firstUpdate;

		double m_prevCurXpos = 0.0, m_prevCurYpos = 0.0;
		float m_fov = 60.0f;
		float m_aspectRatio = 1.0f;
		float m_near = 1.0f, m_far = 100.0f;
		float m_scr_width, m_scr_height;

		CameraMatrix m_cm;

		void SetCameraOrientation();

	public:
		glm::vec3 position = glm::vec3(0.0f, 0.0f, 1.0f);
		glm::vec3 front = glm::vec3(0.0f, 0.0f, 1.0f);
		glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
		glm::vec3 right = glm::vec3(1.0f, 0.0f, 0.0f);
		glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
		float pitch = 0;
		float yaw = 0;
		float roll = 0;


		int i = 0;
		Camera(GLFWwindow* window, glm::vec3 cameraPosition, glm::vec3 cameraFront, glm::vec3 globalUp, float fov, float aspectRatio, float near, float far, float scr_width, float scr_height);
		Camera(GLFWwindow* window, float fov, float aspectRatio, float near, float far, glm::vec3 cameraPosition, glm::vec3 globalUp, glm::vec3 eulerValues = glm::vec3(0.0f, 0.0f, 0.0f));

		CameraMatrix UpdateCamera(float movementSpeed, float rotationSpeed,
			int forwardKey = GLFW_KEY_W,
			int backwardKey = GLFW_KEY_S,
			int leftKey = GLFW_KEY_A,
			int rightKey = GLFW_KEY_D);

		glm::vec3 GetMouseClickDirection();
	};
}
#endif // !CAMERA_H