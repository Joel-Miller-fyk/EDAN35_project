#include "project.hpp"
#include "parametric_shapes.hpp"

#include "config.hpp"
#include "core/Bonobo.h"
#include "core/FPSCamera.h"
#include "core/node.hpp"
#include "core/helpers.hpp"
#include "core/ShaderProgramManager.hpp"

#include <imgui.h>
#include <tinyfiledialogs.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <array>
#include <clocale>
#include <cstdlib>
#include <stdexcept>
#include <random>

#include <iostream>
#include <fstream>
#include <sstream>

edaf80::project::project(WindowManager& windowManager) :
	mCamera(0.5f * glm::half_pi<float>(),
		static_cast<float>(config::resolution_x) / static_cast<float>(config::resolution_y),
		0.01f, 1000.0f),
	inputHandler(), mWindowManager(windowManager), window(nullptr)
{
	WindowManager::WindowDatum window_datum{ inputHandler, mCamera, config::resolution_x, config::resolution_y, 0, 0, 0, 0 };

	window = mWindowManager.CreateGLFWWindow("EDAN35: project", window_datum, config::msaa_rate);
	if (window == nullptr) {
		throw std::runtime_error("Failed to get a window: aborting!");
	}

	bonobo::init();
}

edaf80::project::~project()
{
	bonobo::deinit();
}

void
edaf80::project::boundaryCollisions(glm::vec3* position, glm::vec3* velocity) {
	glm::vec2 halfBoundSize = glm::vec2(poolWidth / 2 - particleRadius, PoolHeight / 2 - particleRadius);

	if (abs(position->x) > halfBoundSize.x) {
		
		position->x = halfBoundSize.x * (signbit(position->x)?1:-1);
		velocity->x *= -1 * collisionDamping;
	}
	if (abs(position->y) > halfBoundSize.y) {
		position->y = halfBoundSize.y * (signbit(position->x) ? 1 : -1);
		velocity->y *= -1 * collisionDamping;
	}
}
std::string
edaf80::project::readFile(const std::string& filePath) {
	std::ifstream file(filePath);
	if (!file) {
		std::cerr << "Failed to open file: " << filePath << std::endl;
		return "";
	}

	std::ostringstream content;
	content << file.rdbuf();
	return content.str();
}
GLuint
edaf80::project::compileShader(GLenum shaderType, const std::string& source) {
	GLuint shader = glCreateShader(shaderType);
	const char* sourcePtr = source.c_str();
	glShaderSource(shader, 1, &sourcePtr, nullptr);
	glCompileShader(shader);

	// ���������
	GLint success;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		GLchar infoLog[512];
		glGetShaderInfoLog(shader, sizeof(infoLog), nullptr, infoLog);
		std::cerr << "Shader compilation error:\n" << infoLog << std::endl;
		glDeleteShader(shader);
		return 0;
	}

	return shader;
}
GLuint
edaf80::project::createComputeShaderProgram(const std::string& computeShaderSource) {
	// ���������ɫ��
	GLuint computeShader = compileShader(GL_COMPUTE_SHADER, computeShaderSource);
	if (computeShader == 0) {
		std::cerr << "Failed to compile compute shader." << std::endl;
		return 0;
	}
	// �����������
	GLuint computeProgram = glCreateProgram();
	// ���Ӽ�����ɫ�����������
	glAttachShader(computeProgram, computeShader);
	glLinkProgram(computeProgram);
	// ������Ӵ���
	GLint success;
	glGetProgramiv(computeProgram, GL_LINK_STATUS, &success);
	if (!success) {
		GLchar infoLog[512];
		glGetProgramInfoLog(computeProgram, sizeof(infoLog), nullptr, infoLog);
		std::cerr << "Shader program linking error:\n" << infoLog << std::endl;
		glDeleteProgram(computeProgram);
		glDeleteShader(computeShader);
		return 0;
	}
	// ɾ��������Ҫ�ļ�����ɫ��
	glDeleteShader(computeShader);

	return computeProgram;
}
glm::vec2
edaf80::project::CalculateDensity1(std::vector<particleParameter> particles) {
	float sqrRadiaus = smoothingRadius * smoothingRadius;
	float density = 0.0;
	float nearDensity = 0.0;
	int i = 0;
	while (i < particlesNum - 1) {
		i++;
		glm::vec2 neighbourPos = particles[i].predictedPosition;
		glm::vec2 offsetToNeighbour = neighbourPos - particles[i].position;
		float sqrDstToNeighbour = dot(offsetToNeighbour, offsetToNeighbour);

		if (sqrDstToNeighbour > sqrRadiaus) continue;

		float dst = sqrt(sqrDstToNeighbour);
		if (dst < smoothingRadius)
		{
			float v = smoothingRadius - dst;
			float SpikyPow2ScalingFactor = 6 / (pow(smoothingRadius, 4) * pi);
			density += v * v * SpikyPow2ScalingFactor;
		}
		if (dst < smoothingRadius)
		{
			float v = smoothingRadius - dst;
			float SpikyPow3ScalingFactor = 10 / (pow(smoothingRadius, 5) * pi);
			nearDensity += v * v * SpikyPow3ScalingFactor;
		}
	}
	return glm::vec2(density, nearDensity);
}

edaf80::ParticleSpawner::ParticleSpawner()
{
}

edaf80::ParticleSpawner::~ParticleSpawner()
{
}
class ParticleDisplay2D
{
public:
	ParticleDisplay2D();
	~ParticleDisplay2D();

private:

};

ParticleDisplay2D::ParticleDisplay2D()
{
}

ParticleDisplay2D::~ParticleDisplay2D()
{
}

const GLchar* computeShaderSource = R"(
    #version 430 core

    layout(binding = 0, std430) buffer DataBuffer {
        vec2 data[];
    };
	layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

    void main() {
        uint index = gl_GlobalInvocationID.x;
        data[index] += vec2(0.0, -2.0);
    }
)";
const GLchar* computeShaderSource1 = R"(
    #version 430 core

    layout(std430, binding = 0) buffer PositionBuffer {
		vec4 Positions[];
	};
	layout(std430, binding = 1) buffer VelocityBuffer {
		vec4 Velocities[];
	};
	uniform float collisionDamping;
	uniform float gravity;
	uniform float particleRadius;
	uniform float deltaTime;
	uniform vec2 boundsSize;

	layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;
    void main() {
        uint index = gl_GlobalInvocationID.x;
        float2 pos = Positions[particleIndex];
		float2 vel = Velocities[particleIndex];

		// Keep particle inside bounds
		const float2 halfSize = boundsSize * 0.5;
		float2 edgeDst = halfSize - abs(pos);

		if (edgeDst.x <= 0)
		{
				pos.x = halfSize.x * sign(pos.x);
		vel.x *= -1 * collisionDamping;
		}
		if (edgeDst.y <= 0)
		{
			pos.y = halfSize.y * sign(pos.y);
			vel.y *= -1 * collisionDamping;
		}

		// Collide particle against the test obstacle
		const float2 obstacleHalfSize = obstacleSize * 0.5;
		float2 obstacleEdgeDst = obstacleHalfSize - abs(pos - obstacleCentre);

		if (obstacleEdgeDst.x >= 0 && obstacleEdgeDst.y >= 0)
	{
		if (obstacleEdgeDst.x < obstacleEdgeDst.y) {
			pos.x = obstacleHalfSize.x * sign(pos.x - obstacleCentre.x) + obstacleCentre.x;
			vel.x *= -1 * collisionDamping;
		}
		else {
			pos.y = obstacleHalfSize.y * sign(pos.y - obstacleCentre.y) + obstacleCentre.y;
			vel.y *= -1 * collisionDamping;
		}
	}

	// Update position and velocity
	Positions[particleIndex] = pos;
	Velocities[particleIndex] = vel;
    }
)";

void
edaf80::project::run()
{
	//auto const shape = parametric_shapes::createBriefCircleRing(particleRadius, particleRadius * 2, 40u, 4u);
	//if (shape.vao == 0u)
	//	return;
	// Set up the camera
	mCamera.mWorld.SetTranslate(glm::vec3(0.0f, 0.0f, 20.0f));
	mCamera.mMouseSensitivity = glm::vec2(0.003f);
	mCamera.mMovementSpeed = glm::vec3(10.0f); // 3 m/s => 10.8 km/h

	// Create the shader programs
	ShaderProgramManager program_manager;
	GLuint fallback_shader = 0u;
	program_manager.CreateAndRegisterProgram("Fallback",
		{ { ShaderType::vertex, "common/fallbackParticle.vert" },
		  { ShaderType::fragment, "common/fallbackParticle.frag" } },
		fallback_shader);
	if (fallback_shader == 0u) {
		LogError("Failed to load fallback shader");
		return;
	}
	GLuint fallbackBoundary_shader = 0u;
	program_manager.CreateAndRegisterProgram("FallbackBoundary",
		{ { ShaderType::vertex, "common/fallback.vert" },
		  { ShaderType::fragment, "common/fallback.frag" } },
		fallbackBoundary_shader);
	if (fallbackBoundary_shader == 0u) {
		LogError("Failed to load fallbackBoundary shader");
		return;
	}
	//GLuint compute_shader = 0u;
	//program_manager.CreateAndRegisterProgram("Compute",
	//	{ { ShaderType::compute, "common/computeShader.comp" }},
	//	compute_shader);
	//if (compute_shader == 0u) {
	//	LogError("Failed to load compute shader");
	//	return;
	//}

	auto const light_position = glm::vec3(-2.0f, 4.0f, 2.0f);
	auto const set_uniforms = [&light_position](GLuint program) {
		glUniform3fv(glGetUniformLocation(program, "light_position"), 1, glm::value_ptr(light_position));
		};
	//
	// Todo: Insert the creation of other shader programs.
	//       (Check how it was done in assignment 3.)
	//
	std::vector<glm::vec2> positions;
	std::vector<glm::vec2> velocities;
	positions = spawner.GetSpawnData().positions;
	velocities = spawner.GetSpawnData().velocities;
	//for (int i = 0; i < 100; i++) {
	//	velocities[i] += glm::vec2(5.0f, 0.0f);
	//}
	//auto const shape = parametric_shapes::createBriefCircleRingBatch3(particleRadius, particleRadius * 2, 10u, 2u, spawner.particleCount, &positions, &velocities);
	auto const shape = parametric_shapes::createBriefCircleRingBatch2(particleRadius, particleRadius * 2, 10u, 2u, spawner.particleCount, positions, velocities);
	//for (int i = 0; i < 100; i++) {
	//	positions[i] += glm::vec2(1.0f, 0.0f);
	//}
	auto circle = Node();
	circle.set_geometry(shape);
	circle.set_program(&fallback_shader, set_uniforms);
	circle.get_transform().SetTranslate(glm::vec3(0.0f, 0.0f, 0.0f));
	TRSTransformf& circle_rings_transform_ref = circle.get_transform();


	std::vector<particleParameter> particles(spawner.particleCount);
	for (int i = 0; i < spawner.particleCount; i++) {
		particles[i].position = positions[i];
		particles[i].predictedPosition = positions[i];
		particles[i].velocity = velocities[i];
		//particles[i].density = CalculateDensity1(particles);
	}
	//std::cout << sizeof(particleParameter) << std::endl;
	

	//GLuint positionBuffer;
	//glGenBuffers(1, &positionBuffer);
	//glBindBuffer(GL_SHADER_STORAGE_BUFFER, positionBuffer);
	//glBufferData(GL_SHADER_STORAGE_BUFFER, positions.size() * sizeof(glm::vec2), positions.data(), GL_DYNAMIC_DRAW);
	//glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, positionBuffer);
	//
	//GLuint velocityBuffer;
	//glGenBuffers(1, &velocityBuffer);
	//glBindBuffer(GL_SHADER_STORAGE_BUFFER, velocityBuffer);
	//glBufferData(GL_SHADER_STORAGE_BUFFER, velocities.size() * sizeof(glm::vec2), velocities.data(), GL_DYNAMIC_DRAW);
	//glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, velocityBuffer);
	//--------------------------------------------------
	//GLuint positionBuffer;
	//glGenBuffers(1, &positionBuffer);
	//glBindBuffer(GL_SHADER_STORAGE_BUFFER, positionBuffer);
	//glBufferData(GL_ARRAY_BUFFER, 2 * positions.size() * sizeof(glm::vec2), nullptr, GL_DYNAMIC_COPY);

	//glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, positions.size() * sizeof(glm::vec2), positions.data());
	//glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, positionBuffer);

	//GLuint velocityBuffer;
	//glGenBuffers(1, &velocityBuffer);
	//glBindBuffer(GL_SHADER_STORAGE_BUFFER, velocityBuffer);
	//glBufferSubData(GL_SHADER_STORAGE_BUFFER, positions.size() * sizeof(glm::vec2), velocities.size() * sizeof(glm::vec2), velocities.data());
	//glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, velocityBuffer);
	//-----------------------------------
	GLuint particleBuffer;
	glGenBuffers(1, &particleBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, particles.size() * sizeof(particleParameter), particles.data(), GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particleBuffer);

	std::string computeShaderSource = readFile("F:/desktop/CourseFile/ComputerGraphics/src/EDAF80/computeShader5.glsl");
	if (computeShaderSource.empty()) {
		std::cerr << "Failed to read compute shader source file." << std::endl;
		return;
	}
	GLuint computeProgram = createComputeShaderProgram(computeShaderSource);
	if (computeProgram == 0) {
		std::cerr << "Failed to create compute shader program." << std::endl;
		return;
	}
	////calculation part
	//GLuint buffer;
	//glGenBuffers(1, &buffer);
	//glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buffer);
	//glBufferData(GL_SHADER_STORAGE_BUFFER, positions.size() * sizeof(glm::vec2), positions.data(), GL_DYNAMIC_COPY);
	////����compute shader
	//GLuint computeShader = glCreateShader(GL_COMPUTE_SHADER);
	//glShaderSource(computeShader, 1, &computeShaderSource, nullptr);
	//glCompileShader(computeShader);
	//// ���������Ӽ������
	//GLuint computeProgram = glCreateProgram();
	//glAttachShader(computeProgram, computeShader);
	//glLinkProgram(computeProgram);
	//GLint linkStatus;
	//glGetProgramiv(computeProgram, GL_LINK_STATUS, &linkStatus);
	//if (linkStatus != GL_TRUE) {
	//	std::cerr << "Program linking failed\n";
	//	GLchar infoLog[512];
	//	glGetProgramInfoLog(computeProgram, sizeof(infoLog), nullptr, infoLog);
	//	std::cerr << infoLog << std::endl;
	//	LogError("Failed to llink shader program");
	//	return;
	//}
	//---------------------------
	//glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, positionBuffer);
	//glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, velocityBuffer);
	//glUseProgram(computeProgram);// use compute shader ������wg
	////// ���ȼ�������
	//glDispatchCompute(64, 1, 1);//Dispatch
	////glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
	//////get the calculated data
	//glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, positions.size() * sizeof(glm::vec2), positions.data());
	////GLfloat* result = static_cast<GLfloat*>(glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY));
	////for (int i = 0; i < positions.size(); i++) {
	////	positions[i].x = result[2 * i];
	////	positions[i].y = result[2 * i + 1];
	////}
	////glDeleteBuffers(1, &buffer);
	////glDeleteProgram(computeProgram);
	//glUseProgram(0);
	//glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0u);
	//glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0u);
	////glDeleteShader(computeShader);
	//-------------------------------------

	//Set up down left right boundaries
	auto hor_boundary_shape = parametric_shapes::createQuad(17.1f, 0.03f, 10u, 1u);
	auto ver_boundary_shape = parametric_shapes::createQuad(9.0f, 0.03f, 10u, 1u);
	Node up_boundary_node;
	up_boundary_node.set_geometry(hor_boundary_shape);
	up_boundary_node.set_program(&fallbackBoundary_shader, set_uniforms);
	up_boundary_node.get_transform().SetTranslate(glm::vec3(-poolWidth/2, -PoolHeight/2 , 0.0f));
	up_boundary_node.get_transform().SetRotateX(-glm::pi<float>() /2);
	Node down_boundary_node;
	down_boundary_node.set_geometry(hor_boundary_shape);
	down_boundary_node.set_program(&fallbackBoundary_shader, set_uniforms);
	down_boundary_node.get_transform().SetTranslate(glm::vec3(-poolWidth / 2, PoolHeight / 2, 0.0f));
	down_boundary_node.get_transform().SetRotateX(-glm::pi<float>() / 2);
	Node left_boundary_node;
	left_boundary_node.set_geometry(ver_boundary_shape);
	left_boundary_node.set_program(&fallbackBoundary_shader, set_uniforms);
	left_boundary_node.get_transform().SetTranslate(glm::vec3(-poolWidth / 2, -PoolHeight / 2, 0.0f));
	left_boundary_node.get_transform().RotateX(-glm::pi<float>() / 2);
	left_boundary_node.get_transform().RotateZ(glm::pi<float>() / 2);
	Node right_boundary_node;
	right_boundary_node.set_geometry(ver_boundary_shape);
	right_boundary_node.set_program(&fallbackBoundary_shader, set_uniforms);
	right_boundary_node.get_transform().SetTranslate(glm::vec3(poolWidth / 2, -PoolHeight / 2, 0.0f));
	right_boundary_node.get_transform().RotateX(-glm::pi<float>() / 2);
	right_boundary_node.get_transform().RotateZ(glm::pi<float>() / 2);
	//
	// Todo: Load your geometry
	//
	glClearDepthf(1.0f);
	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	glEnable(GL_DEPTH_TEST);


	auto lastTime = std::chrono::high_resolution_clock::now();
	auto frame_last_Time = std::chrono::high_resolution_clock::now();

	bool show_logs = true;
	bool show_gui = true;
	bool shader_reload_failed = false;
	bool show_basis = false;
	float basis_thickness_scale = 1.0f;
	float basis_length_scale = 1.0f;

	//project parameters
	float elapsed_time_s = 0.0f;
	float float_deltaTime = 0;
	glm::vec3 velocity = glm::vec3(0.0f);
	glm::vec3 position = circle.get_transform().GetTranslation();
	//float gravity = 10.0f;
	int frameCount = 0;
	auto polygon_mode = bonobo::polygon_mode_t::fill;




	while (!glfwWindowShouldClose(window)) {
		auto const nowTime = std::chrono::high_resolution_clock::now();
		auto const deltaTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(nowTime - lastTime);

		lastTime = nowTime;
		elapsed_time_s += std::chrono::duration<float>(deltaTimeUs).count();
		float_deltaTime = std::chrono::duration<float>(deltaTimeUs).count();
		
		//calculate fps
		auto const frame_current_Time = std::chrono::high_resolution_clock::now();
		auto const frame_deltaTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(frame_current_Time - frame_last_Time);
		float frame_float_deltaTimeUs = std::chrono::duration<float>(frame_deltaTimeUs).count();
		frameCount++;
		if (frame_float_deltaTimeUs >= 1.0) {
			double fps = static_cast<double>(frameCount) / frame_float_deltaTimeUs;
			std::cout << "FPS: " << fps << std::endl;
			frameCount = 0;
			frame_last_Time = frame_current_Time;
		}
		//update place
		//velocity.y += - gravity * float_deltaTime;
		//position += glm::vec3(0, velocity.y * float_deltaTime, 0);
		//boundaryCollisions(&position, &velocity);
		//circle.get_transform().SetTranslate(position);
		//test
		//for (int i = 0; i < velocities.size(); i++) {
		//	velocities[i] = velocity;
		//}

		auto& io = ImGui::GetIO();
		inputHandler.SetUICapture(io.WantCaptureMouse, io.WantCaptureKeyboard);

		glfwPollEvents();
		inputHandler.Advance();
		mCamera.Update(deltaTimeUs, inputHandler);

		if (inputHandler.GetKeycodeState(GLFW_KEY_R) & JUST_PRESSED) {
			shader_reload_failed = !program_manager.ReloadAllPrograms();
			if (shader_reload_failed)
				tinyfd_notifyPopup("Shader Program Reload Error",
					"An error occurred while reloading shader programs; see the logs for details.\n"
					"Rendering is suspended until the issue is solved. Once fixed, just reload the shaders again.",
					"error");
		}
		if (inputHandler.GetKeycodeState(GLFW_KEY_F3) & JUST_RELEASED)
			show_logs = !show_logs;
		if (inputHandler.GetKeycodeState(GLFW_KEY_F2) & JUST_RELEASED)
			show_gui = !show_gui;
		if (inputHandler.GetKeycodeState(GLFW_KEY_F11) & JUST_RELEASED)
			mWindowManager.ToggleFullscreenStatusForWindow(window);
		glm::vec2 mousePos;
		if (inputHandler.GetMouseState(GLFW_MOUSE_BUTTON_RIGHT) & PRESSED) {
			mousePos = inputHandler.GetMousePosition();
			std::cout << mousePos << std::endl;
		}

		// Retrieve the actual framebuffer size: for HiDPI monitors,
		// you might end up with a framebuffer larger than what you
		// actually asked for. For example, if you ask for a 1920x1080
		// framebuffer, you might get a 3840x2160 one instead.
		// Also it might change as the user drags the window between
		// monitors with different DPIs, or if the fullscreen status is
		// being toggled.
		int framebuffer_width, framebuffer_height;
		glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
		glViewport(0, 0, framebuffer_width, framebuffer_height);


		//
		// Todo: If you need to handle inputs, you can do it here
		//


		mWindowManager.NewImGuiFrame();

		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
		bonobo::changePolygonMode(polygon_mode);

		if (!shader_reload_failed) {
			//
			// Todo: Render all your geometry here.
			//
			//circle.render(mCamera.GetWorldToClipMatrix());
			//circle.render(mCamera.GetWorldToClipMatrix(),position1);
			//for each (glm::vec2 position in positions)
			//{
			//	circle.render(mCamera.GetWorldToClipMatrix(), position);
			//}
			//glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, positionBuffer);
			//glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, velocityBuffer);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particleBuffer);
			glUseProgram(computeProgram);
			glUniform1f(glGetUniformLocation(computeProgram, "collisionDamping") , collisionDamping);
			glUniform1f(glGetUniformLocation(computeProgram, "gravity"), gravity);
			glUniform1f(glGetUniformLocation(computeProgram, "particleRadius"), particleRadius);
			glUniform1f(glGetUniformLocation(computeProgram, "deltaTime"), float_deltaTime);
			glUniform2fv(glGetUniformLocation(computeProgram, "boundsSize"), 1, glm::value_ptr(boundsSize));

			glUniform1i(glGetUniformLocation(computeProgram, "numParticles"), particlesNum);
			glUniform1f(glGetUniformLocation(computeProgram, "smoothingRadius"), smoothingRadius);
			glUniform1f(glGetUniformLocation(computeProgram, "targetDensity"), targetDensity);
			glUniform1f(glGetUniformLocation(computeProgram, "pressureMultiplier"), pressureMultiplier);
			glUniform1f(glGetUniformLocation(computeProgram, "nearPressureMultiplier"), nearPressureMultiplier);
			glUniform1f(glGetUniformLocation(computeProgram, "viscosityStrength"), viscosityStrength);

			glUniform1f(glGetUniformLocation(computeProgram, "interactionInputRadius"), interactionRadius);
			glUniform1f(glGetUniformLocation(computeProgram, "interactionInputStrength"), interactionStrength);
			glUniform2fv(glGetUniformLocation(computeProgram, "interactionInputPoint"), 1, glm::value_ptr(mousePos));
			glDispatchCompute(100, 1, 1);
			//glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
			glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, particles.size() * sizeof(particleParameter), particles.data());
			for (int i = 0; i < spawner.particleCount; i++) {
				positions[i] = particles[i].position;
				velocities[i] = particles[i].velocity;
			}
			//test
			//std::cout << velocities[100] << std::endl;
			//glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, positions.size() * sizeof(glm::vec2), velocities.size() * sizeof(glm::vec2), velocities.data());
			//glDeleteBuffers(1, &buffer);
			//glDeleteProgram(computeProgram);
			glUseProgram(0);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0u);
			//glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0u);

			circle.render(mCamera.GetWorldToClipMatrix(),spawner.particleCount, positions, velocities);
			//up_boundary_node.render(mCamera.GetWorldToClipMatrix());
			//down_boundary_node.render(mCamera.GetWorldToClipMatrix());
			//left_boundary_node.render(mCamera.GetWorldToClipMatrix());
			//right_boundary_node.render(mCamera.GetWorldToClipMatrix());
		}


		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		//
		// Todo: If you want a custom ImGUI window, you can set it up
		//       here
		//
		bool const opened = ImGui::Begin("Scene Controls", nullptr, ImGuiWindowFlags_None);
		if (opened) {
			ImGui::Checkbox("Show basis", &show_basis);
			ImGui::SliderFloat("Basis thickness scale", &basis_thickness_scale, 0.0f, 100.0f);
			ImGui::SliderFloat("Basis length scale", &basis_length_scale, 0.0f, 100.0f);
			bonobo::uiSelectPolygonMode("Polygon mode", polygon_mode);
			ImGui::SliderFloat("Boundary width", &boundsSize.x, 10.0f, 30.0f);
			ImGui::SliderFloat("Boundary height", &boundsSize.y, 5.0f, 20.0f);
		}
		ImGui::End();

		if (show_basis)
			bonobo::renderBasis(basis_thickness_scale, basis_length_scale, mCamera.GetWorldToClipMatrix());
		if (show_logs)
			Log::View::Render();
		mWindowManager.RenderImGuiFrame(show_gui);

		glfwSwapBuffers(window);
	}
}

int main()
{
	std::setlocale(LC_ALL, "");

	Bonobo framework;

	try {
		edaf80::project project(framework.GetWindowManager());
		project.run();
	}
	catch (std::runtime_error const& e) {
		LogError(e.what());
	}
}
