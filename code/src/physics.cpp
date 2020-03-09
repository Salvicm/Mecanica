#include <imgui\imgui.h>
#include <imgui\imgui_impl_sdl_gl3.h>
#include <imgui\imgui_impl_sdl_gl3.h>
#include <glm\glm.hpp>
#include <glm\gtc\type_ptr.hpp>
#include <GL\glew.h>
#include <glm\gtc\matrix_transform.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#if __has_include(<ppl.h>)
#  include <ppl.h>
#  define ppl 1
#else
#  define ppl 0
#endif

float elasticity = 0.75f;
int maxThreads = std::thread::hardware_concurrency();
enum class Mode{FOUNTAIN, CASCADE};
static const char* ModeString[]{ "Fountain", "Cascade" };
enum class ExecutionMode{STANDARD, PARALLEL, MULTITHREADING};
ExecutionMode exMode = ExecutionMode::STANDARD;
#if ppl == 1
static const char* ExecutionModeString[]{ "Standard", "Parallel (EXPERIMENTAL!)", "Multithreading (EXPERIMENTAL!)" };
#else
static const char* ExecutionModeString[]{ "Standard", "Parallel (c++17 not aviable)", "Multithreading (EXPERIMENTAL!)" };
#endif
enum class CascadeAxis{X_LEFT, X_RIGHT, Z_FRONT, Z_BACK};
static const char* CascadeAxisString[]{ "Z left", "X right", "Z front", "Z back" };

//Function declarations
glm::vec4 getRectFormula(glm::vec3 _a, glm::vec3 _b, glm::vec3 _c, glm::vec3 _d);

namespace Sphere {
	extern void setupSphere(glm::vec3 pos, float radius);
	extern void cleanupSphere();
	extern void updateSphere(glm::vec3 pos, float radius);

}
namespace LilSpheres {
	extern void updateParticles(int startIdx, int count, float* array_data);
	extern int particleCount;

}
namespace Box {
	extern GLuint cubeVao;
	extern GLuint cubeVbo[];
	extern GLuint cubeShaders[];
	extern GLuint cubeProgram;

	extern float cubeVerts[];
	extern GLubyte cubeIdx[];
}
glm::vec3 eulerSolver(const glm::vec3 origin, const glm::vec3 end, const float _dt) {
	return origin + _dt * end;
}
glm::vec3 eulerFixer(glm::vec3 origin, glm::vec3 end, glm::vec4 planeInfo) {
	return{ 0,0,0 };
}
bool checkWithPlane(const glm::vec3 originalPos, const glm::vec3 endPos,const glm::vec4 plano) {
	 // (n * pt + d)(n*pt+dt + d)
	float x = (originalPos.x * plano.x) + (originalPos.y *plano.y) + (originalPos.z * plano.z) + plano.w;
	float y = (endPos.x * plano.x) + (endPos.y *plano.y) + (endPos.z * plano.z) + plano.w;
	return x * y < 0;
}
glm::vec3 fixPos(const glm::vec3 originalPos,const glm::vec3 endPos,const glm::vec4 plano) {
	glm::vec3 newPos = { 0,0,0 };
	glm::vec3 normalPlano = { plano.x, plano.y, plano.z };
	newPos = (endPos - (2 * (glm::dot(endPos, normalPlano) + plano.w)) * normalPlano); // Podríamos usar glm::reflect
	return newPos;

}
glm::vec3 fixSpeed(glm::vec3 originalSpeed, glm::vec3 endSpeed, glm::vec4 plano) { 
	glm::vec3 newSpeed = { 0,0,0 };
	glm::vec3 normalPlano = { plano.x, plano.y, plano.z };
	newSpeed = (endSpeed - (2 * (glm::dot(endSpeed, normalPlano))) * normalPlano) * elasticity;

	return newSpeed;
}

struct Spheres {
	float radius;
	glm::vec3 position;
	Spheres() : radius(2.0f), position({ 0,0,0 }) {}
	Spheres(float rad, glm::vec3 pos) : radius(rad), position(pos){}
};
bool CheckCollisionWithSphere(Spheres sphere, glm::vec3 primaPos);
glm::vec4 getPlaneFromSphere(glm::vec3 originalPos, glm::vec3 endPos, Spheres sphere);
void UwU() {

}
struct Particles {
	std::vector<Spheres> spheres = std::vector<Spheres>(1, Spheres{ 2.5f, {0, 2.5f, -0} });

#pragma region BasicParticlesData
	Mode mode = Mode::CASCADE; // UI  --> El selector entre fuente y cascada
	CascadeAxis axis = CascadeAxis::X_RIGHT; // UI --> El selector entre ejes de la cascada
	float distFromAxis = 2.0f; // UI --> 0 - 5 --> Distancia a la pared en cascada
	float cascadeHeight = 5.0f; // UI --> 0 - 9.99 --> Altura de la cascada
	glm::vec3 fountainOrigin = { 0.f, 5.0f,0.f }; // UI --> {(-5,5), (0,9'99), (-5,5)} --> Posicion de origen de la fuente
	glm::vec3 acceleration = { 0, -9.81f, 0 }; // UI --> Aceleracion de todas las partículas
	glm::vec3 *positions;
	glm::vec3 *primaPositions;
	glm::vec3 *speeds;
	glm::vec3 *primaSpeeds;
	float *lifeTime;
	float *currentLifeTime;
	glm::vec3 originalSpeed = { 0,1,0 }; // UI --> Velocidad original para la fuente
	float originalLifetime = 2.5f; // UI --> >=0.5
	// Physics parameters
	// UI --> El minimo y el máximo de la cantidad de partículas
	float min = 0.0f; 
	float max = 1000.0f;
	int maxParticles = 1000; // UI --> Cuando se modifique esto, hacer deletes de todos los arrays dinámicos y llamar a InitParticles
	float maxVisible = 0;
	float emissionRate = 100;
	bool hasStarted = false;
#pragma endregion
	void SetMaxParticles(int n) {
		maxParticles = n;
	}
#pragma region ParticleUpdates
	void InitParticles() {
		srand(time(NULL));
#pragma region sphereInit
		extern bool renderSphere;
		renderSphere = true;
		Sphere::setupSphere(spheres[0].position, spheres[0].radius);
#pragma endregion

	


		positions = new glm::vec3[maxParticles];
		speeds = new glm::vec3[maxParticles];
		primaPositions = new glm::vec3[maxParticles];
		primaSpeeds = new glm::vec3[maxParticles];
		lifeTime = new float[maxParticles];
		currentLifeTime = new float[maxParticles];
		LilSpheres::particleCount = maxParticles;
		extern bool renderParticles;
		renderParticles = true;


		for (int i = 0; i < maxParticles; i++)
		{
			
			float tmpX = (float)(rand() % 500) / 100.f - 2.5f;
			float tmpZ = (float)(rand() % 500) / 100.f - 2.5f;
			float tmpY = 5.f;
			primaSpeeds[i] = speeds[i] = { tmpX, tmpY, tmpZ };
			lifeTime[i] = originalLifetime;
			currentLifeTime[i] = 0;
			glm::vec3 originPosition = { 0,0,0 };
			switch (mode)
			{
			case Mode::FOUNTAIN:
				primaPositions[i] = positions[i] = fountainOrigin;
				break;
			case Mode::CASCADE:
				
				switch (axis)
				{
				case CascadeAxis::X_LEFT:
					originPosition = {-5.f + distFromAxis, cascadeHeight, (((float)(rand() % 100) / 100.f) * 10.f) -5 }; 
					tmpX = (float)(rand() % 250) / 100.f + 2.5f;
					tmpZ = 0.0f;
					tmpY = 1.f;
					primaSpeeds[i] = speeds[i] = { tmpX, tmpY, tmpZ };
					break;
				case CascadeAxis::X_RIGHT:
					originPosition = {+5.f - distFromAxis, cascadeHeight, (((float)(rand() % 100) / 100.f) * 10.f) -5 };
					tmpX = (float)(rand() % 250) / 100.f - 5.f;
					tmpZ = 0.0f;
					tmpY = 1.f;
					primaSpeeds[i] = speeds[i] = { tmpX, tmpY, tmpZ };
					break;
				case CascadeAxis::Z_FRONT:
					originPosition = { (((float)(rand() % 100) / 100.f) * 10.f) - 5 , cascadeHeight, +5.f - distFromAxis }; 
					tmpX = 0.0f;
					tmpZ = (float)(rand() % 250) / 100.f - 5.f;
					tmpY = 1.f;
					primaSpeeds[i] = speeds[i] = { tmpX, tmpY, tmpZ };
					break;
				case CascadeAxis::Z_BACK:
					originPosition = { (((float)(rand() % 100) / 100.f) * 10.f) - 5 , cascadeHeight, -5.f + distFromAxis }; 
					tmpZ = (float)(rand() % 250) / 100.f + 2.5f;
					tmpX = 0.0f;
					tmpY = 1.f;
					primaSpeeds[i] = speeds[i] = { tmpX, tmpY, tmpZ };
					break;
				default:
					break;
				}
				primaPositions[i] = positions[i] = originPosition;
				break;
			default:
				break;
			}
		}
	}
	void UpdateParticles(float dt) {
		// A cada frame inicializar X partículas 
			// Si se superan, no hacer nada
		if (maxVisible < maxParticles && hasStarted) {
			maxVisible += emissionRate * dt; 
		}
		if (maxVisible > maxParticles)
			maxVisible = maxParticles;
		// Mantener un MaxVisible --> Esto afecta a la actualizacion tambien

		hasStarted = true;
		switch (exMode)
		{
		case ExecutionMode::PARALLEL:
#if ppl == 1
			Concurrency::parallel_for(0, (int)maxVisible, [this, dt](int i) {
				UpdateParticle(i, dt);
			});
#endif
			break;
		case ExecutionMode::MULTITHREADING:
		{
			if (maxThreads < 2) maxThreads = 2;
			int calculateParticles = maxVisible / maxThreads;
			int calculatedParticles = 0;
			std::vector<std::thread> threads;
			for (size_t i = 0; i < maxThreads; i++)
			{
				int temp1 = calculatedParticles;
				int temp2 = calculatedParticles + calculateParticles;
				float tempdt = dt;
				std::thread thread(&Particles::UpdateParticlesSection, this, temp1, temp2, tempdt);
				threads.push_back(std::move(thread));
				calculatedParticles += calculateParticles;
			}
			for (size_t i = 0; i < threads.size(); i++)
			{
				threads[i].join();
			}
		}
			break;
		default:
			UpdateParticlesSection(0, maxVisible, dt);
			break;
		}
		LilSpheres::updateParticles(0, maxVisible, &positions[0].x);
	}
	void UpdateParticlesSection(const int & from,const int & to,const float & dt) {
		for (int i = from; i < to; i++)
		{
			UpdateParticle(i, dt);
		}
	}
	void UpdateParticle(int i, float dt) {
		glm::vec4 plano;
		if (i < maxVisible) {
			primaPositions[i] = eulerSolver(positions[i], speeds[i], dt);
			primaSpeeds[i] = eulerSolver(speeds[i], acceleration, dt);


			plano = getRectFormula(
				// Basandonos en los indices del cubo 
				{ Box::cubeVerts[4 * 3], Box::cubeVerts[4 * 3 + 1], Box::cubeVerts[4 * 3 + 2] },
				{ Box::cubeVerts[5 * 3], Box::cubeVerts[5 * 3 + 1], Box::cubeVerts[5 * 3 + 2] },
				{ Box::cubeVerts[6 * 3], Box::cubeVerts[6 * 3 + 1], Box::cubeVerts[6 * 3 + 2] },
				{ Box::cubeVerts[7 * 3], Box::cubeVerts[7 * 3 + 1], Box::cubeVerts[7 * 3 + 2] });
			if (checkWithPlane(positions[i], primaPositions[i], plano)) {
				primaPositions[i] = fixPos(positions[i], primaPositions[i], plano);
				primaSpeeds[i] = fixSpeed(speeds[i], primaSpeeds[i], plano);
			}

			for (int j = 0; j < 20; j += 4) {
				plano = getRectFormula(
					// Basandonos en los indices del cubo 
					{ Box::cubeVerts[Box::cubeIdx[j] * 3], Box::cubeVerts[Box::cubeIdx[j] * 3 + 1], Box::cubeVerts[Box::cubeIdx[j] * 3 + 2] },
					{ Box::cubeVerts[Box::cubeIdx[j + 1] * 3], Box::cubeVerts[Box::cubeIdx[j + 1] * 3 + 1], Box::cubeVerts[Box::cubeIdx[j + 1] * 3 + 2] },
					{ Box::cubeVerts[Box::cubeIdx[j + 2] * 3], Box::cubeVerts[Box::cubeIdx[j + 2] * 3 + 1], Box::cubeVerts[Box::cubeIdx[j + 2] * 3 + 2] },
					{ Box::cubeVerts[Box::cubeIdx[j + 3] * 3], Box::cubeVerts[Box::cubeIdx[j + 3] * 3 + 1], Box::cubeVerts[Box::cubeIdx[j + 3] * 3 + 2] });
				if (checkWithPlane(positions[i], primaPositions[i], plano)) {
					primaPositions[i] = fixPos(positions[i], primaPositions[i], plano);
					primaSpeeds[i] = fixSpeed(speeds[i], primaSpeeds[i], plano);
				}
			}
			extern bool renderSphere;
			if(renderSphere)
				for (auto it = spheres.begin(); it < spheres.end(); it++)
				{
					if (CheckCollisionWithSphere(*it, primaPositions[i])) {
						plano = getPlaneFromSphere(positions[i], primaPositions[i], *it);
						primaPositions[i] = fixPos(positions[i], primaPositions[i], plano);
						primaSpeeds[i] = fixSpeed(speeds[i], primaSpeeds[i], plano);
					}
				}
			speeds[i] = primaSpeeds[i];
			positions[i] = primaPositions[i];

			currentLifeTime[i] += dt;
			if (currentLifeTime[i] >= lifeTime[i]) {
				float x = -5 + min + (float)rand() / (RAND_MAX / (max - min));
				float z = -5 + min + (float)rand() / (RAND_MAX / (max - min));
				float tmpX = (float)(rand() % 500) / 100.f - 2.5f;
				float tmpZ = (float)(rand() % 500) / 100.f - 2.5f;
				float tmpY = 5.f;
				primaSpeeds[i] = speeds[i] = { tmpX, tmpY, tmpZ };
				currentLifeTime[i] = 0;
				glm::vec3 originPosition;
				switch (mode)
				{
				case Mode::FOUNTAIN:
					primaPositions[i] = positions[i] = fountainOrigin;
					break;
				case Mode::CASCADE:

					switch (axis)
					{
					case CascadeAxis::X_LEFT:
						originPosition = { -5.f + distFromAxis, cascadeHeight, (((float)(rand() % 100) / 100.f) * 10.f) - 5 };
						tmpX = (float)(rand() % 250) / 100.f + 2.5f;
						tmpZ = 0.0f;
						tmpY = 1.f;
						primaSpeeds[i] = speeds[i] = { tmpX, tmpY, tmpZ };
						break;
					case CascadeAxis::X_RIGHT:
						originPosition = { +5.f - distFromAxis, cascadeHeight, (((float)(rand() % 100) / 100.f) * 10.f) - 5 };
						tmpX = (float)(rand() % 250) / 100.f - 5.f;
						tmpZ = 0.0f;
						tmpY = 1.f;
						primaSpeeds[i] = speeds[i] = { tmpX, tmpY, tmpZ };
						break;
					case CascadeAxis::Z_FRONT:
						originPosition = { (((float)(rand() % 100) / 100.f) * 10.f) - 5 , cascadeHeight, +5.f - distFromAxis };
						tmpX = 0.0f;
						tmpZ = (float)(rand() % 250) / 100.f - 5.f;
						tmpY = 1.f;
						primaSpeeds[i] = speeds[i] = { tmpX, tmpY, tmpZ };
						break;
					case CascadeAxis::Z_BACK:
						originPosition = { (((float)(rand() % 100) / 100.f) * 10.f) - 5 , cascadeHeight, -5.f + distFromAxis };
						tmpZ = (float)(rand() % 250) / 100.f + 2.5f;
						tmpX = 0.0f;
						tmpY = 1.f;
						primaSpeeds[i] = speeds[i] = { tmpX, tmpY, tmpZ };
						break;
					default:
						break;
					}
					primaPositions[i] = positions[i] = originPosition;


					break;
				default:
					break;
				}
			}
		}
	}
	void CleanParticles() {
		Sphere::cleanupSphere();
		delete[] positions;
		delete[] speeds;
		delete[] lifeTime;
		delete[] currentLifeTime;
		delete[] primaPositions;
		delete[] primaSpeeds;
	}
	void ResetParticles() {
		maxVisible = 0;
		CleanParticles();
		InitParticles();
	}
#pragma endregion
} parts;


bool show_test_window = false;

namespace OriginalSettings {
	Mode mode = parts.mode;

	//CASCADE
	CascadeAxis axis = parts.axis;
	float distFromAxis = parts.distFromAxis;
	float cascadeHeight = parts.cascadeHeight;

	//FOUNTAIN
	glm::vec3 fountainOrigin = parts.fountainOrigin;

	//GENERAL
	glm::vec3 originalSpeed = parts.originalSpeed;
	float originalLifetime = parts.originalLifetime;
	int maxParticles = parts.maxParticles; // UI --> Cuando se modifique esto, hacer deletes de todos los arrays dinámicos y llamar a InitParticles
	float emissionRate = parts.emissionRate;

	//PHYSICS
	float elasticity_ = elasticity;
	glm::vec3 acceleration = parts.acceleration;

	//OBJECTS
	//std::vector<Spheres> spheres = parts.spheres;
	//int sphereCount = parts.spheres.size();

	// UI --> El minimo y el máximo de la cantidad de partículas
	float min = parts.min;
	float max = parts.max;
}
namespace EditedSettings {
	Mode mode = parts.mode;

	//CASCADE
	CascadeAxis axis = parts.axis;
	float distFromAxis = parts.distFromAxis;
	float cascadeHeight = parts.cascadeHeight;

	//FOUNTAIN
	glm::vec3 fountainOrigin = parts.fountainOrigin;

	//GENERAL
	glm::vec3 originalSpeed = parts.originalSpeed;
	float originalLifetime = parts.originalLifetime;
	int maxParticles = parts.maxParticles; // UI --> Cuando se modifique esto, hacer deletes de todos los arrays dinámicos y llamar a InitParticles
	float emissionRate = parts.emissionRate;

	//PHYSICS
	float elasticity_ = elasticity;
	glm::vec3 acceleration = parts.acceleration;

	//OBJECTS
	//std::vector<Spheres> spheres = parts.spheres;
	//int sphereCount = parts.spheres.size();

	// UI --> El minimo y el máximo de la cantidad de partículas
	float min = parts.min;
	float max = parts.max;
}

void GUI() {
	bool show = true;
	ImGui::Begin("Physics Parameters", &show, 0);

	{
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);//FrameRate
	}

	ImGui::NewLine();
	ImGui::Text("System options:");
	ImGui::Combo("Computing mode", (int*)(&exMode), ExecutionModeString, 3);
	ExecutionMode lastExMode = exMode;
	if (exMode == ExecutionMode::MULTITHREADING) {
		std::string maxThreadsLabel = "Max threads ";
		if (maxThreads == std::thread::hardware_concurrency())
			maxThreadsLabel += "(Recommended)";
		ImGui::SliderInt(maxThreadsLabel.c_str(), &maxThreads, 2, std::thread::hardware_concurrency() * 4);
	}
#if ppl == 0
	if (exMode == ExecutionMode::PARALLEL) exMode = ExecutionMode::STANDARD;
#endif
	if(exMode != lastExMode) parts.ResetParticles();
	ImGui::NewLine();
	ImGui::Text("Particle system:");
	std::string spawned = "Spawned particles: " + std::to_string((int)parts.maxVisible);
	ImGui::Text(spawned.c_str());
	ImGui::Text("Emitter options:");
	ImGui::SliderInt("Max Particles", &EditedSettings::maxParticles,100,5000);
	ImGui::Combo("Type", (int*)(&EditedSettings::mode), ModeString, 2);

	if (EditedSettings::mode == Mode::CASCADE) {
		ImGui::Spacing();
		ImGui::Combo("Position", (int*)(&EditedSettings::axis), CascadeAxisString, 4);
		ImGui::SliderFloat("Distance from axis", &EditedSettings::distFromAxis, 0, 5);
		ImGui::SliderFloat("Height", &EditedSettings::cascadeHeight, 0, 9.999f);
		ImGui::Spacing();
	}
	else {
		ImGui::Spacing();
		ImGui::DragFloat3("Position", &EditedSettings::fountainOrigin[0], .01f);
		ImGui::Spacing();
	}
	ImGui::DragFloat3("Start Acceleration", &EditedSettings::originalSpeed[0], .01f);
	ImGui::SliderFloat("Life", &EditedSettings::originalLifetime,.5f,10);
	ImGui::SliderFloat("Emission Rate", &EditedSettings::emissionRate,1,1000);


	ImGui::NewLine();
	ImGui::Text("Physics options:");
	ImGui::SliderFloat("Elasticity", &elasticity, .5f, 1);
	ImGui::DragFloat3("Global Acceleration", &EditedSettings::acceleration[0], .01f);

	ImGui::NewLine();

	if (OriginalSettings::mode != parts.mode
		|| OriginalSettings::axis != parts.axis
		|| OriginalSettings::distFromAxis != parts.distFromAxis
		|| OriginalSettings::cascadeHeight != parts.cascadeHeight
		|| OriginalSettings::fountainOrigin != parts.fountainOrigin
		|| OriginalSettings::originalSpeed != parts.originalSpeed
		|| OriginalSettings::originalLifetime != parts.originalLifetime
		|| OriginalSettings::elasticity_ != elasticity
		|| OriginalSettings::acceleration != parts.acceleration
		|| OriginalSettings::min != parts.min
		|| OriginalSettings::max != parts.max
		|| OriginalSettings::maxParticles != parts.maxParticles
		|| OriginalSettings::emissionRate != parts.emissionRate
		)
	{
		if (ImGui::Button("Reset Settings")) {
			parts.mode = OriginalSettings::mode;
			parts.axis = OriginalSettings::axis;
			parts.distFromAxis = OriginalSettings::distFromAxis;
			parts.cascadeHeight = OriginalSettings::cascadeHeight;
			parts.fountainOrigin = OriginalSettings::fountainOrigin;
			parts.originalSpeed = OriginalSettings::originalSpeed;
			parts.originalLifetime = OriginalSettings::originalLifetime;
			elasticity = OriginalSettings::elasticity_;
			parts.acceleration = OriginalSettings::acceleration;
			parts.min = OriginalSettings::min;
			parts.max = OriginalSettings::max;
			parts.maxParticles = OriginalSettings::maxParticles;
			parts.emissionRate = OriginalSettings::emissionRate;

			EditedSettings::mode = OriginalSettings::mode;
			EditedSettings::axis = OriginalSettings::axis;
			EditedSettings::distFromAxis = OriginalSettings::distFromAxis;
			EditedSettings::cascadeHeight = OriginalSettings::cascadeHeight;
			EditedSettings::fountainOrigin = OriginalSettings::fountainOrigin;
			EditedSettings::originalSpeed = OriginalSettings::originalSpeed;
			EditedSettings::originalLifetime = OriginalSettings::originalLifetime;
			EditedSettings::elasticity_ = OriginalSettings::elasticity_;
			EditedSettings::acceleration = OriginalSettings::acceleration;
			EditedSettings::min = OriginalSettings::min;
			EditedSettings::max = OriginalSettings::max;
			EditedSettings::maxParticles = OriginalSettings::maxParticles;
			EditedSettings::emissionRate = OriginalSettings::emissionRate;

			parts.ResetParticles();
		}
	}
	else {
		ImGui::Spacing();
		ImGui::Text(" Reset Settings");
	}

	if (EditedSettings::mode != parts.mode
		|| EditedSettings::axis != parts.axis
		|| EditedSettings::distFromAxis != parts.distFromAxis
		|| EditedSettings::cascadeHeight != parts.cascadeHeight
		|| EditedSettings::fountainOrigin != parts.fountainOrigin
		|| EditedSettings::originalSpeed != parts.originalSpeed
		|| EditedSettings::originalLifetime != parts.originalLifetime
		|| EditedSettings::elasticity_ != elasticity
		|| EditedSettings::acceleration != parts.acceleration
		|| EditedSettings::min != parts.min
		|| EditedSettings::max != parts.max
		|| EditedSettings::maxParticles != parts.maxParticles
		|| EditedSettings::emissionRate != parts.emissionRate
		)
	{
		if (ImGui::Button("Apply Settings")) {
			parts.mode = EditedSettings::mode;
			parts.axis = EditedSettings::axis;
			parts.distFromAxis = EditedSettings::distFromAxis;
			parts.cascadeHeight = EditedSettings::cascadeHeight;
			parts.fountainOrigin = EditedSettings::fountainOrigin;
			parts.originalSpeed = EditedSettings::originalSpeed;
			parts.originalLifetime = EditedSettings::originalLifetime;
			elasticity = EditedSettings::elasticity_;
			parts.acceleration = EditedSettings::acceleration;
			parts.min = EditedSettings::min;
			parts.max = EditedSettings::max;
			parts.maxParticles = EditedSettings::maxParticles;
			parts.emissionRate = EditedSettings::emissionRate;
			parts.ResetParticles();
		}
	}
	else {
		ImGui::Spacing();
		ImGui::Text(" Apply Settings");
	}

	ImGui::NewLine();
	ImGui::Text("Objects:");
	ImGui::NewLine();
	ImGui::Text("Sphere:");
	//if (ImGui::Button("+")) {
	//	parts.spheres.push_back(Spheres{ 2.5f, {0, 2.5f, -0} });
	//}
	//if (ImGui::Button("-")) {
	//	parts.spheres.pop_back();
	//}
	//Sphere::cleanupSphere();
	extern bool renderSphere;
	ImGui::Checkbox("Enable", &renderSphere);
	for (size_t i = 0; i < parts.spheres.size(); i++)
	{
		ImGui::Spacing();
		ImGui::SliderFloat("Radius", &parts.spheres[i].radius, 0, 10);
		ImGui::DragFloat3("Position", &parts.spheres[i].position[0], .01f);
		Sphere::updateSphere(parts.spheres[i].position, parts.spheres[i].radius);
	}
	ImGui::NewLine();
	ImGui::Text("Capsule:");
	ImGui::End();
}

void PhysicsInit() {
	parts.InitParticles();
}

void PhysicsUpdate(float dt) {
	parts.UpdateParticles(dt);
}

void PhysicsCleanup() {
	parts.CleanParticles();
}
glm::vec4 getRectFormula(glm::vec3 _a, glm::vec3 _b, glm::vec3 _c, glm::vec3 _d) {
	float A, B, C, D ;
	A = B =C =  D = 0.0f;
	// Teniendo los puntos, obtener dos vectores para el plano
	glm::vec3 VecA = _b - _a; // Recta A = _a + x*VecA
	glm::vec3 VecB = _b - _c; // Recta B = _c + x*VecB
	glm::vec3 Punto = _a;
	// Sacar la normal
	/*
	|i,  j,	 k | <-- Obtener el vector perpendicular a otros dos vectores
	|Ax, Ay, Az|
	|Bx, By, Bz|= (a)-(b)+(c) = 0
	*/
	A = (VecA.y * VecB.z) - (VecB.y * VecA.z);
	B = (VecA.x * VecB.z) - (VecB.x * VecA.z);
	C = (VecA.x * VecB.y) - (VecB.x * VecA.y);

	glm::vec3 Normal = { A, -B, C };
	// Normalizar vector
	Normal = glm::normalize(Normal); // Ahora calcular la D
	// Ax + By + Cz + D = 0;
	// D = -Ax - By - Cz
	D = (Normal.x * Punto.x * -1) + (Normal.y * Punto.y * -1) + (Normal.z * Punto.z * -1);
	return{ Normal.x,Normal.y, Normal.z,D }; // Esto me retorna la fórmula general del plano, ya normalizado
	
}
bool CheckCollisionWithSphere(Spheres sphere, glm::vec3 primaPos) {
	return glm::distance(sphere.position, primaPos) <= sphere.radius;
}

glm::vec4 getPlaneFromSphere(glm::vec3 originalPos, glm::vec3 endPos, Spheres sphere) {

	// Vector director
		// f(x) = originalPos + lambda * Director;  
	glm::vec3 director = endPos - originalPos; 
	/* 
	|| Pcol	 - C ||^2 = r^2 --> PCol = OP + lV
	|| OP +lV - C ||^2 = r^2
	|| lV + (OP-C) ||^2 ...
	|| lV + CP || ^2 = r^2
	dot(lv+CP, lv+CP) = r^2 
	dot(lv, lv+CP)+dot(CP,(lV+CP)) = r2;
	l^2 = dot(v,v) 
	l = 2*dot(v, CP)
	- = dot(CP,CP) - r2
	*/
	glm::vec3 CP = originalPos -sphere.position;
	float square = glm::dot(director,director);
	float normal = glm::dot(director, CP) + glm::dot(CP, director);
	float number = glm::dot(CP,CP) - (sphere.radius*sphere.radius);

	double sqrt = glm::sqrt((normal*normal) - (4 * square*number));


	double lambda = glm::min((-normal + sqrt) / 2 * square, (-normal - sqrt) / 2 * square);
	// Punto de impacto
	glm::vec3 impact = originalPos + glm::vec3{ lambda * director.x, lambda * director.y, lambda * director.z}; // Si, me daba pereza sobreescribir la multiplicacion por la del double
	// Plano tangente
	glm::vec3 normalPlano = glm::normalize(impact-sphere.position);
	float D = (normalPlano.x * impact.x * -1) + (normalPlano.y * impact.y * -1) + (normalPlano.z * impact.z * -1);
	return { normalPlano.x, normalPlano.y, normalPlano.z, D};
}
