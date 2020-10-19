// Generic Utility
#include <iostream>
#include <string>
#include <chrono>

// Custom
#include <shader.h>
#include <logger.h>
#include <model.h>
#include <camera.h>
#include <lights.h>
#include <renderer.h>

// Essentials
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtx/string_cast.hpp>
#include <sstream>

static xre::LogModule* LOGGER = xre::LogModule::getLoggerInstance();
const unsigned int SCR_WIDTH = 1600, SCR_HEIGHT = 900;
void framebufferSizeCallback(GLFWwindow* window, int width, int height); // Do not define any type other than 'int' for width / height parameters. e.g. Incorrect : const unsigned int& width, Correct : int width.

GLFWwindow* GLFWWindowManager(const int& major_version, const int& minor_version, const GLenum& opengl_profile)
{
	GLFWwindow* window = nullptr;

	// GLFW Inintiallization and Configuration
	try
	{
		glfwInit();
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, major_version);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, minor_version);
		glfwWindowHint(GLFW_OPENGL_PROFILE, opengl_profile);
		glfwWindowHint(GLFW_SAMPLES, 4);

#ifdef __APPLE__
		glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
		LOGGER->log(xre::LOG_LEVEL::INFO, "GLFW Initiallization", "Initiallization successful (Profile - Core, Version " + std::to_string(major_version) + "." + std::to_string(minor_version) + ")");
	}
	catch (...)
	{
		LOGGER->log(xre::LOG_LEVEL::FATAL, "GLFW Initiallization", "Initiallization unsuccessful!");
	}

	// GLFW Window Creation
	window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Xperimental Rendering Engine", NULL, NULL);

	if (window == NULL)
	{
		LOGGER->log(xre::LOG_LEVEL::FATAL, "GLFW Window Creation", "Failed to create GLFW Window!");
		glfwTerminate();
	}
	else
	{
		LOGGER->log(xre::LOG_LEVEL::INFO, "GLFW Window Creation", "Window created successfully.");


		glfwMakeContextCurrent(window);
		glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

		// GLFW capture mouse
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	}
	return window;
}

int main()
{
	// Set logging modules log level
	LOGGER->setLogLevel(xre::LOG_LEVEL::INFO, xre::LOG_LEVEL_FILTER_TYPE::GREATER_OR_EQUAL);

	// GLFW initiallization and  Window Creation
	GLFWwindow* window = GLFWWindowManager(3, 3, GLFW_OPENGL_CORE_PROFILE);
	if (window == nullptr)
	{
		return -1;
	}

	// glad: load all OpenGL function pointers
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		LOGGER->log(xre::LOG_LEVEL::FATAL, "GLAD", "Failed to initiallize.");
		return -1;
	}

	xre::RenderSystem* renderer = xre::RenderSystem::renderer(SCR_WIDTH, SCR_HEIGHT, glm::vec4(0.8f, 0.8f, 0.8f, 1.0f), 0.01f, 100.0f, 3048, 3048);
	renderer->SwitchPfx(false);

	// Camera creation
	xre::Camera camera(window, glm::vec3(0.0f, 2.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 70.0f, (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.01f, 100.0f, SCR_WIDTH, SCR_HEIGHT);
	xre::CameraMatrix cm;

	// Lights setup
	xre::DirectionalLight directional_light = xre::DirectionalLight(glm::vec3(0.0f, 50.0f, 0.0f), glm::vec3(0.8f, 0.8f, 0.8f), 2.0f, "directionalLight");

	xre::PointLight point_light_0 = xre::PointLight(glm::vec3(0.0f, 8.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), 1, 0.022f, 0.0019f, 2.0f, "pointLights[0]");

	glm::vec3 model_placements[] = {
		glm::vec3(0.0f, 0.0f, 0.0f),
		glm::vec3(1.0f, -2.0f, -2.0f),
		glm::vec3(2.5f, 6.0f, -4.0f),
		glm::vec3(3.0f, 1.0f, 1.0f),
		glm::vec3(-0.5f, 0.0f, 2.0f)
	};

	// Imported object render test
	// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

	xre::Model ferrari("D:/Work/Xperimental Rendering Engine/XRE/Source/Resources/Models/41-formula-1/formula 1/Formula 1 mesh.fbx", "ferrari", aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_MakeLeftHanded | aiProcess_CalcTangentSpace | aiProcess_GenNormals);
	xre::Shader ferrari_shader("D:/Work/Xperimental Rendering Engine/XRE/Source/Resources/Shaders/vertex_shader.vert", "D:/Work/Xperimental Rendering Engine/XRE/Source/Resources/Shaders/fragment_shader.frag");
	ferrari.translate(glm::vec3(0.0f));
	ferrari.scale(glm::vec3(0.01f));

	xre::Model backpack("D:/Work/Xperimental Rendering Engine/XRE/Source/Resources/Models/backpack/backpack.obj", "backpack", aiProcess_Triangulate | aiProcess_CalcTangentSpace);
	xre::Shader backpack_shader("D:/Work/Xperimental Rendering Engine/XRE/Source/Resources/Shaders/vertex_shader.vert", "D:/Work/Xperimental Rendering Engine/XRE/Source/Resources/Shaders/fragment_shader.frag");
	backpack.translate(glm::vec3(-1.0f, 1.0f, 0.0f));
	backpack.scale(glm::vec3(0.1f));

	xre::Model sponza("D:/Work/Xperimental Rendering Engine/XRE/Source/Resources/Models/crytek-sponza-huge-vray-obj/crytek-sponza-huge-vray.obj", "sponza", aiProcess_Triangulate | aiProcess_CalcTangentSpace | aiProcess_DropNormals | aiProcess_FlipUVs | aiProcess_GenNormals | aiProcess_FixInfacingNormals);
	xre::Shader sponza_shader("D:/Work/Xperimental Rendering Engine/XRE/Source/Resources/Shaders/vertex_shader.vert", "D:/Work/Xperimental Rendering Engine/XRE/Source/Resources/Shaders/fragment_shader.frag");
	sponza.translate(glm::vec3(0.0, 0.0, 0.0));
	sponza.scale(glm::vec3(0.01f));

	// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------


	// Additional data
	std::chrono::duration<float> delta_time;
	// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

	bool first_refresh = false;

	while (!glfwWindowShouldClose(window))
	{
		auto start = std::chrono::high_resolution_clock::now();

		if (!first_refresh)
		{                                                                                   
			cm = camera.UpdateCamera(2.0f * delta_time.count(), 20.0f * delta_time.count());
			first_refresh = true;
		}
		cm = camera.UpdateCamera(2.0f * delta_time.count(), 20.0f * delta_time.count());

		renderer->setCameraMatrices(&cm.view, &cm.projection, &camera.position);
		// Ferrai draw call
		ferrari_shader.setMat4("view", cm.view);
		ferrari_shader.setMat4("projection", cm.projection);
		ferrari_shader.setMat4("model", ferrari.model_matrix);
		ferrari_shader.setVec3("camera_position_vertex", camera.position);
		//ferrari_shader.setVec3("camera_pos", camera.position);
		ferrari_shader.setFloat("shininess", 128);
		ferrari.draw(ferrari_shader, "ferrari");

		// Backpack draw call
		backpack.rotate(glm::cos(glm::radians(glfwGetTime())) * 0.01f, glm::vec3(1.0f));
		backpack_shader.setMat4("view", cm.view);
		backpack_shader.setMat4("projection", cm.projection);
		backpack_shader.setMat4("model", backpack.model_matrix);
		backpack_shader.setVec3("camera_position_vertex", camera.position);
		//backpack_shader.setVec3("camera_pos", camera.position);
		backpack_shader.setFloat("shininess", 32);
		backpack.draw(backpack_shader, "backpack");

		// Sponza draw call
		sponza_shader.setMat4("view", cm.view);
		sponza_shader.setMat4("projection", cm.projection);
		sponza_shader.setMat4("model", sponza.model_matrix);
		sponza_shader.setVec3("camera_position_vertex", camera.position);
		//sponza_shader.setVec3("camera_pos", camera.position);
		sponza_shader.setFloat("shininess", 128);
		sponza.draw(sponza_shader, "sponza");

		// Draw to screen
		renderer->drawToScreen();

		// glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
		glfwSwapBuffers(window);
		glfwPollEvents();

		delta_time = std::chrono::high_resolution_clock::now() - start;

		//std::cout << 1 / delta_time.count() << "\n";
	}

	glfwTerminate();
	return 0;
}

void framebufferSizeCallback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
}