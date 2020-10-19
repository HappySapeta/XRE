#include <camera.h>

#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>

using namespace xre;

Camera::Camera(GLFWwindow* window, glm::vec3 cameraPosition, glm::vec3 cameraFront, glm::vec3 globalUp, float fov, float aspectRatio, float near, float far, float scr_width, float scr_height)
{
	position = cameraPosition;
	front = cameraFront;
	worldUp = globalUp;

	m_cm = { glm::mat4(1.0f), glm::mat4(1.0f) };

	Camera::m_window = window;
	Camera::m_fov = fov;
	Camera::m_aspectRatio = aspectRatio;
	Camera::m_near = near;
	Camera::m_far = far;
	Camera::m_scr_width = scr_width;
	Camera::m_scr_height = scr_height;

	m_firstUpdate = true;
}

Camera::Camera(GLFWwindow* window, float fov, float aspectRatio, float near, float far, glm::vec3 cameraPosition, glm::vec3 globalUp, glm::vec3 eulerValues)
{
	position = cameraPosition;
	worldUp = globalUp;
	pitch = eulerValues.x;
	yaw = eulerValues.y;
	roll = eulerValues.z;

	m_cm = { glm::mat4(1.0f), glm::mat4(1.0f) };

	Camera::m_window = window;
	Camera::m_fov = fov;
	Camera::m_aspectRatio = aspectRatio;
	Camera::m_near = near;
	Camera::m_far = far;

	SetCameraOrientation();
	m_firstUpdate = true;
}

void Camera::SetCameraOrientation()
{
	front = glm::vec3(glm::cos(glm::radians(pitch)) * glm::cos(glm::radians(yaw)), glm::sin(glm::radians(pitch)), glm::cos(glm::radians(pitch)) * glm::sin(glm::radians(yaw)));
	right = glm::cross(front, worldUp);
	up = glm::cross(right, front);
}

CameraMatrix Camera::UpdateCamera(float movementSpeed, float rotationSpeed,
	int forwardKey,
	int backwardKey,
	int leftKey,
	int rightKey)
{
	double xPos, yPos;

	right = glm::cross(front, worldUp);
	up = glm::cross(right, front);

	if (glfwGetKey(m_window, forwardKey) == GLFW_PRESS)
		position = position + front * movementSpeed;
	if (glfwGetKey(m_window, backwardKey) == GLFW_PRESS)
		position = position - front * movementSpeed;
	if (glfwGetKey(m_window, leftKey) == GLFW_PRESS)
		position = position - right * movementSpeed;
	if (glfwGetKey(m_window, rightKey) == GLFW_PRESS)
		position = position + right * movementSpeed;

	glfwGetCursorPos(m_window, &xPos, &yPos);

	if (m_firstUpdate)
	{
		m_prevCurXpos = xPos;
		m_prevCurYpos = yPos;
		m_firstUpdate = false;
	}

	float xDelta = (float)(xPos - m_prevCurXpos);
	float yDelta = (float)(-yPos + m_prevCurYpos);

	m_prevCurXpos = xPos;
	m_prevCurYpos = yPos;

	pitch += yDelta * rotationSpeed;
	yaw += xDelta * rotationSpeed;

	if (pitch > 89.99f)
		pitch = 89.99f;
	if (pitch < -89.99f)
		pitch = -89.99f;

	front.x = glm::cos(glm::radians(pitch)) * glm::cos(glm::radians(yaw));
	front.y = glm::sin(glm::radians(pitch));
	front.z = glm::cos(glm::radians(pitch)) * glm::sin(glm::radians(yaw));

	front = glm::normalize(front);

	m_projection = glm::perspective(glm::radians(m_fov), m_aspectRatio, m_near, m_far);
	m_view = glm::lookAt(position, position + front, up);

	m_cm.projection = m_projection;
	m_cm.view = m_view;

	return m_cm;
}

glm::vec3 Camera::GetMouseClickDirection()
{
	double xPos, yPos, zPos;
	glfwGetCursorPos(m_window, &xPos, &yPos);

	xPos /= m_scr_width;
	yPos /= m_scr_height;

	xPos = (xPos - 0.5) * 2;
	yPos = (yPos - 0.5) * 2;

	glm::vec4 ray_clip = glm::vec4(xPos, yPos, -1.0, 1.0);

	glm::vec4 ray_eye = glm::inverse(m_projection) * ray_clip;
	ray_eye = glm::vec4(ray_eye.x, ray_eye.y, -1.0, 1.0);

	glm::vec4 ray_world4 = glm::inverse(m_view) * ray_eye;
	glm::vec3 ray_world = glm::normalize(glm::vec3(ray_world4.x, ray_world4.y, ray_world4.z));

	return ray_world;
}