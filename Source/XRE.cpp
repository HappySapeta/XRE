// Generic Utility
#include <iostream>
#include <string>
#include <chrono>
#include <filesystem>
#include <assimp/version.h>

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

#ifdef __APPLE__
		glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
		LOGGER->log(xre::LOG_LEVEL::INFO, "GLFW Initiallization", "Initiallization successful (Profile - Core, Version " + std::to_string(major_version) + "." + std::to_string(minor_version) + ")");
	}
	catch (...)
	{
		LOGGER->log(xre::LOG_LEVEL::FATAL, "GLFW Initiallization", "Initiallization unsuccessful!");
	}

	// GLFW Window CreationB
	int count;
	GLFWmonitor** monitors = glfwGetMonitors(&count);

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
	std::cout << "Origin : " << std::filesystem::current_path() << "\n";
	std::cout << "Assimp verison : " << aiGetVersionMajor() << "." << aiGetVersionMinor() << "\n";
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

	xre::RENDER_PIPELINE rendering_pipeline = xre::RENDER_PIPELINE::DEFERRED;
	xre::LIGHTING_MODE lighting_mode = xre::LIGHTING_MODE::PBR; // PBR works with deferred only.

	xre::Renderer* renderer = xre::Renderer::renderer(SCR_WIDTH, SCR_HEIGHT,
		glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
		1.0f, 100.0f, 512, 512,
		rendering_pipeline, lighting_mode);

	// Camera creation
	xre::Camera camera(window, glm::vec3(0.0f, 2.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 60.0f, (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f, SCR_WIDTH, SCR_HEIGHT);
	xre::CameraMatrix cm;

	// Lights setup
	//xre::DirectionalLight directional_light = xre::DirectionalLight(glm::vec3(8.0f, 50.0f, -28.0f), glm::vec3(1.0f, 1.0f, 1.0f), 40.0f, "directionalLight");

	xre::PointLight point_light_0 = xre::PointLight(glm::vec3(0.0f, 2.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), 1, 0.7f, 1.8f, 35.0f, "pointLights[0]");
	xre::PointLight point_light_1 = xre::PointLight(glm::vec3(8.0f, 2.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), 1, 0.7f, 1.8f, 35.0f, "pointLights[1]");
	xre::PointLight point_light_2 = xre::PointLight(glm::vec3(-8.0f, 2.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), 1, 0.7f, 1.8f, 35.0f, "pointLights[2]");
	//xre::PointLight point_light_3 = xre::PointLight(glm::vec3(0.0f, 2.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), 1, 0.7f, 1.8f, 35.0f, "pointLights[3]");
	//xre::PointLight point_light_4 = xre::PointLight(glm::vec3(8.0f, 2.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), 1, 0.7f, 1.8f, 35.0f, "pointLights[4]");
	//xre::PointLight point_light_5 = xre::PointLight(glm::vec3(-8.0f, 2.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), 1, 0.7f, 1.8f, 35.0f, "pointLights[5]");
	//xre::PointLight point_light_6 = xre::PointLight(glm::vec3(0.0f, 2.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), 1, 0.7f, 1.8f, 35.0f, "pointLights[6]");
	//xre::PointLight point_light_7 = xre::PointLight(glm::vec3(8.0f, 2.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), 1, 0.7f, 1.8f, 35.0f, "pointLights[7]");
	//xre::PointLight point_light_8 = xre::PointLight(glm::vec3(-8.0f, 2.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), 1, 0.7f, 1.8f, 35.0f, "pointLights[8]");
	//xre::PointLight point_light_9 = xre::PointLight(glm::vec3(0.0f, 2.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), 1, 0.7f, 1.8f, 35.0f, "pointLights[9]");
	//xre::PointLight point_light_10 = xre::PointLight(glm::vec3(8.0f, 2.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), 1, 0.7f, 1.8f, 35.0f, "pointLights[10]");
	//xre::PointLight point_light_11 = xre::PointLight(glm::vec3(-8.0f, 2.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), 1, 0.7f, 1.8f, 35.0f, "pointLights[11]");
	//xre::PointLight point_light_12 = xre::PointLight(glm::vec3(0.0f, 2.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), 1, 0.7f, 1.8f, 35.0f, "pointLights[12]");
	//xre::PointLight point_light_13 = xre::PointLight(glm::vec3(8.0f, 2.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), 1, 0.7f, 1.8f, 35.0f, "pointLights[13]");
	//xre::PointLight point_light_14 = xre::PointLight(glm::vec3(-8.0f, 2.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), 1, 0.7f, 1.8f, 35.0f, "pointLights[14]");
	//xre::PointLight point_light_15 = xre::PointLight(glm::vec3(0.0f, 2.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), 1, 0.7f, 1.8f, 35.0f, "pointLights[15]");
	//xre::PointLight point_light_16 = xre::PointLight(glm::vec3(8.0f, 2.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), 1, 0.7f, 1.8f, 35.0f, "pointLights[16]");
	//xre::PointLight point_light_17 = xre::PointLight(glm::vec3(-8.0f, 2.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f), 1, 0.7f, 1.8f, 35.0f, "pointLights[17]");



	// Imported object render test
	// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

	xre::Model backpack("./Source/Resources/Models/backpack_PBR/backpack.obj", "backpack",
		aiProcess_Triangulate
		| aiProcess_CalcTangentSpace
		| aiProcess_OptimizeMeshes
		| aiProcess_GenBoundingBoxes
		| aiProcess_OptimizeGraph);
	xre::Shader* backpack_shader = NULL;
	backpack.dynamic = true;
	backpack.translate(glm::vec3(-1.0f, 1.0f, 0.0f));
	backpack.scale(glm::vec3(0.1f));

	xre::Model sponza("./Source/Resources/Models/sponza_PBR/sponza.obj", "sponza",
		aiProcess_Triangulate
		| aiProcess_CalcTangentSpace
		| aiProcess_OptimizeMeshes
		| aiProcess_OptimizeGraph
		| aiProcess_ForceGenNormals
		| aiProcess_GenBoundingBoxes
		| aiProcess_FlipUVs);
	xre::Shader* sponza_shader = NULL;
	sponza.translate(glm::vec3(0.0, 0.0, 0.0));
	sponza.scale(glm::vec3(0.01f));
	sponza.dynamic = false;

	if (rendering_pipeline == xre::RENDER_PIPELINE::FORWARD)
	{
		backpack_shader = new xre::Shader("./Source/Resources/Shaders/BlinnPhong/forward_bphong_vertex_shader.vert",
			"./Source/Resources/Shaders/BlinnPhong/forward_bphong_fragment_shader.frag");
		sponza_shader = new xre::Shader("./Source/Resources/Shaders/BlinnPhong/forward_bphong_vertex_shader.vert",
			"./Source/Resources/Shaders/BlinnPhong/forward_bphong_fragment_shader.frag");
	}

	// Additional data
	std::chrono::duration<float> delta_time;
	// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

	// Push objects to draw queue
	backpack.draw(*backpack_shader, "backpack");
	sponza.draw(*sponza_shader, "sponza");
	// ----------------------------------------
	while (!glfwWindowShouldClose(window))
	{
		LOGGER->log(xre::INFO, "XRE", "Now rendering...");

		auto start = std::chrono::high_resolution_clock::now();
		//directional_light.m_position = glm::vec3(0.0f, 50.0f, 0.0f) + glm::vec3(30 * glm::cos(glm::radians(glfwGetTime() * 5.0)), 0.0, 30 * glm::sin(glm::radians(glfwGetTime() * 5.0)));

		//point_light_0.m_position = glm::vec3(0.0f, 2.0f, 0.0f) + glm::vec3(3.0 * glm::cos(glm::radians(glfwGetTime() * 20.0)), 0.0, 0.0);
		//point_light_1.m_position = glm::vec3(8.0f, 2.0f, 0.0f) + glm::vec3(3.0 * glm::cos(glm::radians(glfwGetTime() * 20.0)), 0.0, 0.0);
		//point_light_2.m_position = glm::vec3(-8.0f, 2.0f, 0.0f) + glm::vec3(3.0 * glm::cos(glm::radians(glfwGetTime() * 20.0)), 0.0, 0.0);

		cm = camera.UpdateCamera(4.0f * delta_time.count(), 20.0f * delta_time.count());
		renderer->setCameraMatrices(&cm.view, &cm.projection, &camera.position, &camera.front);
		backpack.rotate(glm::cos(glm::radians(glfwGetTime())) * 0.001, glm::vec3(1.0));

		if (rendering_pipeline == xre::RENDER_PIPELINE::FORWARD)
		{
			backpack_shader->use();
			backpack_shader->setMat4("view", cm.view);
			backpack_shader->setMat4("projection", cm.projection);
			backpack_shader->setMat4("model", backpack.model_matrix);
			backpack_shader->setVec3("camera_position_vertex", camera.position);
			backpack_shader->setFloat("shininess", 32);
			backpack_shader->setFloat("positive_exponent", 20.0f);
			backpack_shader->setFloat("negative_exponent", 80.0f);

			sponza_shader->use();
			sponza_shader->setMat4("view", cm.view);
			sponza_shader->setMat4("projection", cm.projection);
			sponza_shader->setMat4("model", sponza.model_matrix);
			sponza_shader->setVec3("camera_position_vertex", camera.position);
			sponza_shader->setFloat("shininess", 128);
			backpack_shader->setFloat("positive_exponent", 20.0f);
			backpack_shader->setFloat("negative_exponent", 80.0f);
		}

		// Draw to screen
		renderer->Render();
		// glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
		glfwSwapBuffers(window);

		delta_time = std::chrono::high_resolution_clock::now() - start;

		glfwPollEvents();
	}

	glfwTerminate();
	return 0;
}

void framebufferSizeCallback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
}