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
const unsigned int SCR_WIDTH = 1920, SCR_HEIGHT = 1080;
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
	GLFWwindow* window = GLFWWindowManager(4, 4, GLFW_OPENGL_CORE_PROFILE);
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

	bool deferred = false; // Make this false, for forward shading.

	xre::RenderSystem* renderer = xre::RenderSystem::renderer(SCR_WIDTH, SCR_HEIGHT, deferred,glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), 1.0f, 100.0f, 1024 * 2, 2 * 1024);
	renderer->SwitchPfx(false);

	// Camera creation
	xre::Camera camera(window, glm::vec3(0.0f, 2.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 70.0f, (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.01f, 100.0f, SCR_WIDTH, SCR_HEIGHT);
	xre::CameraMatrix cm;

	// Lights setup
	//xre::DirectionalLight directional_light = xre::DirectionalLight(glm::vec3(0.0f, 20.0f, 0.0f), glm::vec3(0.8f, 0.8f, 0.8f), 10.0f, "directionalLight");

	xre::PointLight point_light_0 = xre::PointLight(glm::vec3(0.0f, 2.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), 1, 0.7f, 1.8f, 10.0f, "pointLights[0]");
	xre::PointLight point_light_1 = xre::PointLight(glm::vec3(8.0f, 2.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), 1, 0.7f, 1.8f, 25.0f, "pointLights[1]");
	xre::PointLight point_light_2 = xre::PointLight(glm::vec3(-8.0f, 2.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 1, 0.7f, 1.8f, 25.0f, "pointLights[2]");

	// Imported object render test
	// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

	xre::Model ferrari("D:/Github/XperimentalRenderingEngine/XRE/Source/Resources/Models/formula1/formula1/Formula_1_mesh.fbx", "ferrari",
		aiProcess_Triangulate
		| aiProcess_CalcTangentSpace
		| aiProcess_FlipUVs
		| aiProcess_OptimizeMeshes
		| aiProcess_OptimizeGraph);
	xre::Shader* ferrari_shader = NULL;
	ferrari.dynamic = false;
	ferrari.translate(glm::vec3(0.0f));
	ferrari.scale(glm::vec3(0.01f));

	xre::Model backpack("D:/Github/XperimentalRenderingEngine/XRE/Source/Resources/Models/backpack/backpack.obj", "backpack",
		aiProcess_Triangulate
		| aiProcess_CalcTangentSpace
		| aiProcess_OptimizeMeshes
		| aiProcess_OptimizeGraph);
	xre::Shader* backpack_shader = NULL;
	backpack.dynamic = true;
	backpack.translate(glm::vec3(-1.0f, 1.0f, 0.0f));
	backpack.scale(glm::vec3(0.1f));

	xre::Model sponza("D:/Github/XperimentalRenderingEngine/XRE/Source/Resources/Models/sponza/sponza.obj", "sponza",
		aiProcess_Triangulate
		| aiProcess_CalcTangentSpace
		| aiProcess_OptimizeMeshes
		| aiProcess_OptimizeGraph
		| aiProcess_ForceGenNormals
		| aiProcess_FlipUVs);
	xre::Shader* sponza_shader = NULL;
	sponza.translate(glm::vec3(0.0, 0.0, 0.0));
	sponza.scale(glm::vec3(0.01f));
	sponza.dynamic = false;

	if (!deferred)
	{
		ferrari_shader = new xre::Shader("D:/Github/XperimentalRenderingEngine/XRE/Source/Resources/Shaders/vertex_shader.vert",
			"D:/Github/XperimentalRenderingEngine/XRE/Source/Resources/Shaders/fragment_shader.frag");
		backpack_shader = new xre::Shader("D:/Github/XperimentalRenderingEngine/XRE/Source/Resources/Shaders/vertex_shader.vert",
			"D:/Github/XperimentalRenderingEngine/XRE/Source/Resources/Shaders/fragment_shader.frag");
		sponza_shader = new xre::Shader("D:/Github/XperimentalRenderingEngine/XRE/Source/Resources/Shaders/vertex_shader.vert",
			"D:/Github/XperimentalRenderingEngine/XRE/Source/Resources/Shaders/fragment_shader.frag");
	}

	// Additional data
	std::chrono::duration<float> delta_time;
	// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

	// Push objects to draw queue
	// ----------------------------------------
	ferrari.draw(*ferrari_shader, "ferrari");
	backpack.draw(*backpack_shader, "backpack");
	sponza.draw(*sponza_shader, "sponza");
	// ----------------------------------------

	while (!glfwWindowShouldClose(window))
	{
		LOGGER->log(xre::INFO, "XRE", "Now rendering...");

		auto start = std::chrono::high_resolution_clock::now();
		//directional_light.m_position = glm::vec3(0.0f, 50.0f, 0.0f) + glm::vec3(30 * glm::cos(glm::radians(glfwGetTime() * 5.0)), 0.0, 30 * glm::sin(glm::radians(glfwGetTime() * 5.0)));
		cm = camera.UpdateCamera(2.0f * delta_time.count(), 20.0f * delta_time.count());
		renderer->setCameraMatrices(&cm.view, &cm.projection, &camera.position);
		backpack.rotate(glm::cos(glm::radians(glfwGetTime())) * 0.001f, glm::vec3(1.0f));

		if (!deferred)
		{
			ferrari_shader->use();
			ferrari_shader->setMat4("view", cm.view);
			ferrari_shader->setMat4("projection", cm.projection);
			ferrari_shader->setMat4("model", ferrari.model_matrix);
			ferrari_shader->setVec3("camera_position_vertex", camera.position);
			ferrari_shader->setFloat("shininess", 128);

			backpack_shader->use();
			backpack_shader->setMat4("view", cm.view);
			backpack_shader->setMat4("projection", cm.projection);
			backpack_shader->setMat4("model", backpack.model_matrix);
			backpack_shader->setVec3("camera_position_vertex", camera.position);
			backpack_shader->setFloat("shininess", 32);

			sponza_shader->use();
			sponza_shader->setMat4("view", cm.view);
			sponza_shader->setMat4("projection", cm.projection);
			sponza_shader->setMat4("model", sponza.model_matrix);
			sponza_shader->setVec3("camera_position_vertex", camera.position);
			sponza_shader->setFloat("shininess", 128);
		}

		// Draw to screen
		renderer->drawToScreen();
		// glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
		glfwSwapBuffers(window);

		delta_time = std::chrono::high_resolution_clock::now() - start;
		//std::cout << (int)(1 / delta_time.count()) << "\n";

		glfwPollEvents();
	}

	glfwTerminate();
	return 0;
}

void framebufferSizeCallback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
}