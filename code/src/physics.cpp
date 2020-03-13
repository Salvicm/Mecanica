#include <imgui\imgui.h>
#include <imgui\imgui_impl_sdl_gl3.h>
#include <imgui\imgui_impl_sdl_gl3.h>
#include <glm\glm.hpp>
#include <glm\gtc\type_ptr.hpp>
#include <GL\glew.h>
#include <glm\gtc\matrix_transform.hpp>
#include <glm/gtx/closest_point.hpp>
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

float elasticity = 0.75f; // Aviso, bajar esto de 0.6 puede causar partículas que se salten colisiones si la aceleracion es muy alta.
int maxThreads = std::thread::hardware_concurrency();
enum class Mode{FOUNTAIN, CASCADE_FACES, CASCADE_POINTS};
static const char* ModeString[]{ "Fountain", "Cascade by faces", "Cascade by points" };
enum class ExecutionMode{STANDARD, PARALLEL, MULTITHREADING};
ExecutionMode exMode = ExecutionMode::PARALLEL;
#if ppl == 1
static const char* ExecutionModeString[]{ "Sequential", "Parallel", "Multithreading (EXPERIMENTAL!)" };
#else
static const char* ExecutionModeString[]{ "Sequential", "Parallel (c++17 not available)", "Multithreading (EXPERIMENTAL!)" };
#endif
enum class CascadeAxis{X_LEFT, X_RIGHT, Z_FRONT, Z_BACK};
static const char* CascadeAxisString[]{ "X left", "X right", "Z front", "Z back" };

// Forward declarations
glm::vec4 getRectFormula(glm::vec3 _a, glm::vec3 _b, glm::vec3 _c, glm::vec3 _d);
glm::vec3 GetCascadeRotation(glm::vec3 dir, float angle);
glm::vec3 getPerpVector(glm::vec3 _a, glm::vec3 _b);
glm::vec3 eulerSolver(const glm::vec3 origin, const glm::vec3 end, const float _dt);
bool checkWithPlane(const glm::vec3 originalPos, const glm::vec3 endPos, const glm::vec4 plano);
glm::vec3 fixPos(const glm::vec3 originalPos, const glm::vec3 endPos, const glm::vec4 plano);
glm::vec3 fixSpeed(glm::vec3 originalSpeed, glm::vec3 endSpeed, glm::vec4 plano);

namespace Sphere {
	extern void setupSphere(glm::vec3 pos, float radius);
	extern void cleanupSphere();
	extern void updateSphere(glm::vec3 pos, float radius);

}
namespace Capsule {
	extern void setupCapsule(glm::vec3 posA, glm::vec3 posB, float radius);
	extern void cleanupCapsule();
	extern void updateCapsule(glm::vec3 posA, glm::vec3 posB, float radius);
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


struct Spheres {
	float radius;
	glm::vec3 position;
	Spheres() : radius(2.0f), position({ 0,0,0 }) {}
	Spheres(float rad, glm::vec3 pos) : radius(rad), position(pos){}
};
struct Capsules {
	float radius;
	glm::vec3 position1;
	glm::vec3 position2;
};
bool CheckCollisionWithSphere(Spheres sphere, glm::vec3 primaPos);
bool CheckCollisionWithCapsule(Capsules capsule, glm::vec3 primaPos, glm::vec3 originPos);
glm::vec4 getPlaneFromSphere(glm::vec3 originalPos, glm::vec3 endPos, Spheres sphere);
glm::vec4 getPlaneFromCapsule(glm::vec3 originalPos, glm::vec3 endPos, Capsules capsule);

struct Particles {
	Spheres sphere = { 2.5f, {0, 2.5f, -0} };
	Capsules capsule = { 2, {0,0,-2} , {0,0,2} };
	std::vector<glm::vec4> planes;
#pragma region BasicParticlesData
	Mode mode = Mode::CASCADE_FACES; // UI  --> El selector entre fuente y cascada
	CascadeAxis axis = CascadeAxis::X_RIGHT; // UI --> El selector entre ejes de la cascada
	float distFromAxis = 2.0f; // 0 - 5 
	float cascadeHeight = 5.0f; // 0 - 9.99 
	glm::vec3 fountainOrigin = { 0.f, 5.0f,0.f }; // {(-5,5), (0,9'99), (-5,5)}
	glm::vec3 acceleration = { 0, -9.81f, 0 }; 
	glm::vec3 *positions;
	glm::vec3 *primaPositions;
	glm::vec3 *speeds;
	glm::vec3 *primaSpeeds;
	float *lifeTime;
	float *currentLifeTime;
	glm::vec3 originalSpeed = { -2,0,0 }; 
	const glm::vec3 FountainOriginalSpeed = { 0,5,0 };
	const glm::vec3 CascadeFXOriginalSpeed = { 2,0,0 };
	const glm::vec3 CascadeBXOriginalSpeed = { -2,0,0 };
	const glm::vec3 CascadeFZOriginalSpeed = { 0,0,2 };
	const glm::vec3 CascadeBZOriginalSpeed = { 0,0,-2 };

	glm::vec3 CascadePointA = {-5,0,-5};
	glm::vec3 CascadePointB = { 5,10,5 };
	float cascadeAngle = 0.0f;
	float cascadeStrength = 5.0f;
	float overture = 0.5f;
	float pointsOverture = 0.0f;
	float originalLifetime = 2.5f; //  >=0.5
	
	// Physics parameters
	float min = 0.0f; 
	float max = 1000.0f;
	float emissionRate = 100;
	int maxParticles = emissionRate * originalLifetime; 
	float maxVisible = 0;
	bool hasStarted = false;
#pragma endregion
	void SetMaxParticles(int n) {
		maxParticles = n;
	}
#pragma region ParticleUpdates
	void InitParticles() {
		srand(time(NULL));
#pragma region sphereInit
		Sphere::setupSphere(sphere.position, sphere.radius);
		Capsule::setupCapsule(capsule.position1, capsule.position2, capsule.radius);
		if (planes.size() == 0) {
			planes.push_back(getRectFormula(
				// Basandonos en los indices del cubo 
				{ Box::cubeVerts[4 * 3], Box::cubeVerts[4 * 3 + 1], Box::cubeVerts[4 * 3 + 2] },
				{ Box::cubeVerts[5 * 3], Box::cubeVerts[5 * 3 + 1], Box::cubeVerts[5 * 3 + 2] },
				{ Box::cubeVerts[6 * 3], Box::cubeVerts[6 * 3 + 1], Box::cubeVerts[6 * 3 + 2] },
				{ Box::cubeVerts[7 * 3], Box::cubeVerts[7 * 3 + 1], Box::cubeVerts[7 * 3 + 2] }));
			for (int j = 0; j < 20; j += 4) {
				planes.push_back(getRectFormula(
					// Basandonos en los indices del cubo 
					{ Box::cubeVerts[Box::cubeIdx[j] * 3], Box::cubeVerts[Box::cubeIdx[j] * 3 + 1], Box::cubeVerts[Box::cubeIdx[j] * 3 + 2] },
					{ Box::cubeVerts[Box::cubeIdx[j + 1] * 3], Box::cubeVerts[Box::cubeIdx[j + 1] * 3 + 1], Box::cubeVerts[Box::cubeIdx[j + 1] * 3 + 2] },
					{ Box::cubeVerts[Box::cubeIdx[j + 2] * 3], Box::cubeVerts[Box::cubeIdx[j + 2] * 3 + 1], Box::cubeVerts[Box::cubeIdx[j + 2] * 3 + 2] },
					{ Box::cubeVerts[Box::cubeIdx[j + 3] * 3], Box::cubeVerts[Box::cubeIdx[j + 3] * 3 + 1], Box::cubeVerts[Box::cubeIdx[j + 3] * 3 + 2] }));
			}
			std::cout << planes.size();
		}


#pragma endregion

	


		positions = new glm::vec3[maxParticles];
		speeds = new glm::vec3[maxParticles];
		primaPositions = new glm::vec3[maxParticles];
		primaSpeeds = new glm::vec3[maxParticles];
		lifeTime = new float[maxParticles];
		currentLifeTime = new float[maxParticles];
		LilSpheres::particleCount = 0;
		extern bool renderParticles;
		renderParticles = true;

		for (int i = 0; i < maxParticles; i++)
		{
			
			SpawnParticle(i);
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
		LilSpheres::particleCount = maxVisible;

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

		
			
			extern bool renderSphere;
			if(renderSphere)
				if (CheckCollisionWithSphere(sphere, primaPositions[i])) {
					plano = getPlaneFromSphere(positions[i], primaPositions[i], sphere);
					primaPositions[i] = fixPos(positions[i], primaPositions[i], plano);
					primaSpeeds[i] = fixSpeed(speeds[i], primaSpeeds[i], plano);
				}
			extern bool renderCapsule;
			if(renderCapsule)
				if (CheckCollisionWithCapsule(capsule, primaPositions[i], positions[i])) {
					plano = getPlaneFromCapsule(positions[i], primaPositions[i], capsule);
					primaPositions[i] = fixPos(positions[i], primaPositions[i], plano);
					primaSpeeds[i] = fixSpeed(speeds[i], primaSpeeds[i], plano);
				}
			for (int it = 0; it < planes.size(); it++) {
				// Para evitar problemas, comprobar siempre los planos lo último
				if (checkWithPlane(positions[i], primaPositions[i], planes[it])) {
					primaPositions[i] = fixPos(positions[i], primaPositions[i], planes[it]);
					primaSpeeds[i] = fixSpeed(speeds[i], primaSpeeds[i], planes[it]);
				}
			}


			speeds[i] = primaSpeeds[i];
			positions[i] = primaPositions[i];

			currentLifeTime[i] += dt;
			if (currentLifeTime[i] >= lifeTime[i]) {
				SpawnParticle(i);
			}
		}
	}
	void SpawnParticle(int i) {
		
			float tmpX = originalSpeed.x;
			float tmpY = originalSpeed.y;
			float tmpZ = originalSpeed.z;
			primaSpeeds[i] = speeds[i] = { tmpX, tmpY, tmpZ };
			lifeTime[i] = originalLifetime;
			currentLifeTime[i] = 0;
			glm::vec3 originPosition = { 0,0,0 };
			switch (mode)
			{
			case Mode::FOUNTAIN:
				primaPositions[i] = positions[i] = fountainOrigin;
				if (overture >= 0.01f) {
					tmpY += static_cast <float> (rand()) / static_cast <float> (RAND_MAX)* overture * 10 - (overture * 5);
					tmpX += static_cast <float> (rand()) / static_cast <float> (RAND_MAX)* overture * 10 - (overture * 5);
					tmpZ += static_cast <float> (rand()) / static_cast <float> (RAND_MAX)* overture * 10 - (overture * 5);
				}
				primaSpeeds[i] = speeds[i] = { tmpX, tmpY, tmpZ };
				break;
			case Mode::CASCADE_FACES:
				switch (axis)
				{
				case CascadeAxis::X_LEFT:
					originPosition = {-5.f + distFromAxis, cascadeHeight, ((static_cast <float> (rand()) / static_cast <float> (RAND_MAX)) * 10.f) -5 };
					if (overture >= 0.01f) {
						tmpY += static_cast <float> (rand()) / static_cast <float> (RAND_MAX)* overture * 5 - (overture * 2.5f);
						tmpX += static_cast <float> (rand()) / static_cast <float> (RAND_MAX)* overture * 5 - (overture * 2.5f);
					}
					primaSpeeds[i] = speeds[i] = { tmpX, tmpY, tmpZ };
					break;
				case CascadeAxis::X_RIGHT:
					originPosition = {+5.f - distFromAxis, cascadeHeight, ((static_cast <float> (rand()) / static_cast <float> (RAND_MAX)) * 10.f) -5 };
					if (overture >= 0.01f) {
						tmpY += static_cast <float> (rand()) / static_cast <float> (RAND_MAX)* overture * 5 - (overture * 2.5f);
						tmpX += static_cast <float> (rand()) / static_cast <float> (RAND_MAX)* overture * 5 - (overture * 2.5f);
					}
					primaSpeeds[i] = speeds[i] = { tmpX, tmpY, tmpZ };
					break;
				case CascadeAxis::Z_FRONT:
					originPosition = { ((static_cast <float> (rand()) / static_cast <float> (RAND_MAX)) * 10.f) - 5 , cascadeHeight, +5.f - distFromAxis };
					if (overture >= 0.01f) {
						tmpY += static_cast <float> (rand()) / static_cast <float> (RAND_MAX)* overture * 5 - (overture * 2.5f);
						tmpZ += static_cast <float> (rand()) / static_cast <float> (RAND_MAX)* overture * 5 - (overture * 2.5f);
					}
					primaSpeeds[i] = speeds[i] = { tmpX, tmpY, tmpZ };
					break;
				case CascadeAxis::Z_BACK:
					originPosition = { ((static_cast <float> (rand()) / static_cast <float> (RAND_MAX)) * 10.f) - 5 , cascadeHeight, -5.f + distFromAxis };
					if (overture >= 0.01f) {
						tmpY += static_cast <float> (rand()) / static_cast <float> (RAND_MAX)* overture * 5 - (overture * 2.5f);
						tmpZ += static_cast <float> (rand()) / static_cast <float> (RAND_MAX)* overture * 5 - (overture * 2.5f);
					}
					primaSpeeds[i] = speeds[i] = { tmpX, tmpY, tmpZ };
					break;
				default:
					break;
				}
				primaPositions[i] = positions[i] = originPosition;
				break;
			case Mode::CASCADE_POINTS:
				{
				glm::vec3 director;
				if (CascadePointA != CascadePointB) {

					director = CascadePointA - CascadePointB;
				}
				else {
					director = { 0.01f,0,0 };
				}
				float alpha = (static_cast <float> (rand()) / static_cast <float> (RAND_MAX) * glm::length(director));
				director = glm::normalize(director);
				originPosition = { CascadePointB + alpha * director };
				primaPositions[i] = positions[i] = originPosition;
				float apertureRand = static_cast <float> (rand()) / static_cast <float> (RAND_MAX)* pointsOverture - pointsOverture/2;
				glm::vec3 helper = GetCascadeRotation(director, cascadeAngle + apertureRand);
				primaSpeeds[i] = speeds[i] = helper * cascadeStrength;
				}
				break;
			default:
				break;
			}
	}
	void CleanParticles() {
		Sphere::cleanupSphere();
		Capsule::cleanupCapsule();
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


void GUI() {
	bool show = true;
	ImGui::Begin("Physics Parameters", &show, 0);

	{
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);//FrameRate
	}

	ImGui::NewLine();
	ImGui::Text("System options:");
	ImGui::Combo("Computing mode", (int*)(&exMode), ExecutionModeString, 3);
	if (exMode == ExecutionMode::MULTITHREADING) {
		std::string maxThreadsLabel = "Max threads ";
		if (maxThreads == std::thread::hardware_concurrency())
			maxThreadsLabel += "(Recommended)";
		ImGui::SliderInt(maxThreadsLabel.c_str(), &maxThreads, 2, std::thread::hardware_concurrency() * 4);
	}
#if ppl == 0
	if (exMode == ExecutionMode::PARALLEL) exMode = ExecutionMode::STANDARD;
#endif
	ImGui::NewLine();
	ImGui::Text("Particle system:");
	std::string spawned = "Spawned particles: " + std::to_string((int)parts.maxVisible) + " / " + std::to_string((int)parts.maxParticles);
	ImGui::Text(spawned.c_str());
	ImGui::Text("Emitter options:");
	Mode lastMode = parts.mode;
	CascadeAxis lastAxis = parts.axis;
	ImGui::Combo("Type", (int*)(&parts.mode), ModeString, 3);

	ImGui::Spacing();
	switch (parts.mode)
	{
	case Mode::FOUNTAIN:
		ImGui::DragFloat3("Position", &parts.fountainOrigin[0], .01f);
		ImGui::SliderFloat("Overture", &parts.overture, 0, 1, "%.2f", .5f);

		break;
	case Mode::CASCADE_FACES:
		ImGui::Combo("Position", (int*)(&parts.axis), CascadeAxisString, 4);
		ImGui::SliderFloat("Overture", &parts.overture, 0, 1, "%.2f", .5f);
		ImGui::SliderFloat("Distance from axis", &parts.distFromAxis, 0, 5);
		ImGui::SliderFloat("Height", &parts.cascadeHeight, 0.0001f, 9.999f);

		break;
	case Mode::CASCADE_POINTS:
		ImGui::DragFloat3(("Cascade Point A"), &parts.CascadePointA[0], .01f);
		ImGui::DragFloat3(("Cascade Point B"), &parts.CascadePointB[0], .01f);
		parts.CascadePointA.x = glm::clamp(parts.CascadePointA.x, -4.999f, 4.999f);
		parts.CascadePointA.z = glm::clamp(parts.CascadePointA.z, -4.999f, 4.999f);
		parts.CascadePointB.x = glm::clamp(parts.CascadePointB.x, -4.999f, 4.999f);
		parts.CascadePointB.z = glm::clamp(parts.CascadePointB.z, -4.999f, 4.999f);
		parts.CascadePointA.y = glm::clamp(parts.CascadePointA.y, 0.01f, 9.999f);
		parts.CascadePointB.y = glm::clamp(parts.CascadePointB.y, 0.01f, 9.999f);
		ImGui::SliderFloat("Overture", &parts.pointsOverture, 0, 90, "%.2f", .5f);
		ImGui::SliderFloat("Emition angle", &parts.cascadeAngle, 0, 360);
		ImGui::SliderFloat("Emition force", &parts.cascadeStrength, 0.5, 10);
		break;
	}
	ImGui::Spacing();
	if (lastMode != parts.mode || lastAxis != parts.axis) {

		switch (parts.mode)
		{
		case Mode::FOUNTAIN:
			parts.originalSpeed = parts.FountainOriginalSpeed;
			break;
		case Mode::CASCADE_FACES:
			switch (parts.axis)
			{
			case CascadeAxis::X_LEFT:
				parts.originalSpeed = parts.CascadeFXOriginalSpeed;
				break;
			case CascadeAxis::X_RIGHT:
				parts.originalSpeed = parts.CascadeBXOriginalSpeed;
				break;
			case CascadeAxis::Z_BACK:
				parts.originalSpeed = parts.CascadeFZOriginalSpeed;
				break;
			case CascadeAxis::Z_FRONT:
				parts.originalSpeed = parts.CascadeBZOriginalSpeed;
				break;
			}
			break;
		case Mode::CASCADE_POINTS:

			break;
		}
	}
	if (parts.overture < 0.01f) parts.overture = 0;
	if (parts.pointsOverture < 0.01f) parts.pointsOverture = 0;
	ImGui::DragFloat3("Start Acceleration", &parts.originalSpeed[0], .01f);
	float lastLife = parts.originalLifetime;
	float lastEmission = parts.emissionRate;
	ImGui::SliderFloat("Life", &parts.originalLifetime,1, 10, "%.2f", 2);
	ImGui::SliderFloat("Emission Rate", &parts.emissionRate,1,1000, "%.2f", 2);
	if (parts.originalLifetime != lastLife || parts.emissionRate != lastEmission) {
		parts.maxParticles = parts.originalLifetime * parts.emissionRate;
		parts.ResetParticles();
	}

	if (parts.maxVisible < parts.maxParticles) {
		if (ImGui::Button("Force apply")) {
			parts.ResetParticles();
		}
	}else{
		if (ImGui::Button("Reset")) {
			parts.ResetParticles();
		}
	}


	ImGui::NewLine();
	ImGui::Text("Physics options:");
	ImGui::SliderFloat("Elasticity", &elasticity, .5f, 1);
	ImGui::DragFloat3("Global Acceleration", &parts.acceleration[0], .01f);


	ImGui::NewLine();
	ImGui::Text("Objects:");
	ImGui::NewLine();
	ImGui::Text("Sphere:");

	extern bool renderSphere;
	ImGui::Checkbox("Enable", &renderSphere);
	if (renderSphere) {
		ImGui::Spacing();
		ImGui::SliderFloat("Radius", &parts.sphere.radius, 0, 10);
		ImGui::DragFloat3("Position ", &parts.sphere.position[0], .01f);
		Sphere::updateSphere(parts.sphere.position, parts.sphere.radius);
	}
	ImGui::NewLine();
	extern bool renderCapsule;
	ImGui::Text("Capsule:");
	ImGui::Checkbox("Enable ", &renderCapsule);
	if (renderCapsule) {
		ImGui::Spacing();
		ImGui::SliderFloat("Radius ", &parts.capsule.radius, 0, 10);
		ImGui::DragFloat3("Position 1", &parts.capsule.position1[0], .01f);
		ImGui::DragFloat3("Position 2", &parts.capsule.position2[0], .01f);
		Capsule::updateCapsule(parts.capsule.position1, parts.capsule.position2, parts.capsule.radius);
	}
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

glm::vec3 GetCascadeRotation(glm::vec3 k, float angle) {

	// Obtenemos un vector perpendicular cualquiera, basándonos en el que ya tenemos, esta es la forma mas simple
	glm::vec3 v = { k.y, -k.x, 0 };

	// Aplicamos la "Rodrigues Rotation Formula" que nos permite rotar un vector cualquiera en un eje personalizado
	// v' = v cosθ + (k x v)sinθ + k(k*v)(1-cosθ)  <--- Esta es la formula
	// Donde K = el vector director de la cascada, y V es el vector que queremos rotar
	glm::vec3 newRotation = v * cos(glm::radians(angle)) + (glm::cross(k, v) * sin(glm::radians(angle))) + k * (glm::dot(k, v)) * (1 - cos(glm::radians(angle)));
	return glm::normalize(newRotation); // Lo normalizamos para usarlo en base a la strength
}


glm::vec4 getRectFormula(glm::vec3 _a, glm::vec3 _b, glm::vec3 _c, glm::vec3 _d) {
	// Teniendo los puntos, obtener dos vectores para el plano
	glm::vec3 VecA = _b - _a; // Recta A = _a + x*VecA
	glm::vec3 VecB = _b - _c; // Recta B = _c + x*VecB
	glm::vec3 Punto = _a;


	glm::vec3 Normal = getPerpVector(VecA, VecB);
	Normal = glm::normalize(Normal); // Ahora calcular la D
	// Ax + By + Cz + D = 0;
	// D = -Ax - By - Cz
	float D = (Normal.x * Punto.x * -1) + (Normal.y * Punto.y * -1) + (Normal.z * Punto.z * -1);
	return{ Normal.x,Normal.y, Normal.z,D }; // Esto me retorna la f�rmula general del plano, ya normalizado
	
}
bool CheckCollisionWithSphere(Spheres sphere, glm::vec3 primaPos) {
	return glm::distance(sphere.position, primaPos) <= sphere.radius;
}
bool CheckCollisionWithCapsule(Capsules capsule, glm::vec3 primaPos, glm::vec3 originPos) {
	if (glm::distance(glm::closestPointOnLine(primaPos, capsule.position1, capsule.position2), primaPos) <= capsule.radius)
		if (glm::distance(glm::closestPointOnLine(originPos, capsule.position1, capsule.position2), originPos) > capsule.radius)
			return true;
	
	return false;
}

glm::vec4 getPlaneFromSphere(glm::vec3 originalPos, glm::vec3 endPos, Spheres sphere) {

	// Vector director
		// f(x) = originalPos + lambda * Director;  
	glm::vec3 director = endPos - originalPos; 
	/*  // NO BORRAR --> Explicacion de como funciona el calculo de la esfera
	|| Pcol		-	C		|| ^2 = r^2 --> PCol = OP + lV
	||OP + lV	-	C		|| ^2 = r^2
	||	lV		+	(OP-C)	|| ^2 ...
	||	lV		+	CP		|| ^2 = r^2
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


glm::vec3 getPerpVector(glm::vec3 _a, glm::vec3 _b) {
	// Obtener un vector perpendicular a ambos
		/*
		|i,  j,	 k | <-- Obtener el vector perpendicular a otros dos vectores
		|Ax, Ay, Az|
		|Bx, By, Bz|= (a)-(b)+(c) = 0
		*/
	return { (_a.y * _b.z) - (_b.y * _a.z), (_a.x * _b.z) - (_b.x * _a.z), (_a.x * _b.y) - (_b.x * _a.y) };

}

glm::vec4 getPlaneFromCapsule(glm::vec3 originalPos, glm::vec3 endPos, Capsules capsule) {

	/*
		Obtener los puntos mas cercanos tanto antes como despues de la colision
		Conseguir el punto intermedio entre ambos puntos en la recta y entre la particula antes y despues
		Crear un vector director entre ambos puntos centrales
		El vector director es la normal del plano
		Debido a que en Euler las actualizaciones de colision son siempre lineales, esto nos sirve para hacerlo, en caso de 
			usar un solver mas concreto, daría errores
		Normalizarlo y calcular la D
	
	*/

	glm::vec3 collisionPointInside = glm::closestPointOnLine(endPos, capsule.position1, capsule.position2);
	glm::vec3 collisionPointOutside = glm::closestPointOnLine(originalPos, capsule.position1, capsule.position2);
	glm::vec3 middleColPoint = (collisionPointInside + collisionPointOutside) / 2.0f;
	glm::vec3 middlePartPoint = (originalPos + endPos) / 2.0f;
	glm::vec3 directorPoint = middlePartPoint - middleColPoint;

	directorPoint = glm::normalize(directorPoint);
	float D = (directorPoint.x * middlePartPoint.x * -1) + (directorPoint.y * middlePartPoint.y * -1) + (directorPoint.z * middlePartPoint.z * -1);

	return { directorPoint.x,directorPoint.y,directorPoint.z,D };
}

glm::vec3 eulerSolver(const glm::vec3 origin, const glm::vec3 end, const float _dt) {
	return origin + _dt * end;
}

bool checkWithPlane(const glm::vec3 originalPos, const glm::vec3 endPos, const glm::vec4 plano) {
	// (n * pt + d)(n*pt+dt + d)
	float x = (originalPos.x * plano.x) + (originalPos.y * plano.y) + (originalPos.z * plano.z) + plano.w;
	float y = (endPos.x * plano.x) + (endPos.y * plano.y) + (endPos.z * plano.z) + plano.w;
	return x * y < 0;
}
glm::vec3 fixPos(const glm::vec3 originalPos, const glm::vec3 endPos, const glm::vec4 plano) {
	glm::vec3 newPos = { 0,0,0 };
	glm::vec3 normalPlano = { plano.x, plano.y, plano.z };
	newPos = (endPos - (2 * (glm::dot(endPos, normalPlano) + plano.w)) * normalPlano); // Podr�amos usar glm::reflect
	return newPos;

}
glm::vec3 fixSpeed(glm::vec3 originalSpeed, glm::vec3 endSpeed, glm::vec4 plano) {
	glm::vec3 newSpeed = { 0,0,0 };
	glm::vec3 normalPlano = { plano.x, plano.y, plano.z };
	newSpeed = (endSpeed - (2 * (glm::dot(endSpeed, normalPlano))) * normalPlano) * elasticity;

	return newSpeed;
}


