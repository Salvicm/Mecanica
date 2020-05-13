#include <imgui\imgui.h>
#include <imgui\imgui_impl_sdl_gl3.h>

#include <glm\glm.hpp>
#include <glm\gtc\type_ptr.hpp>
#include <GL\glew.h>
#include <glm\gtc\matrix_transform.hpp>
#include <glm\gtx\closest_point.hpp>
#include <glm\gtc\quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <time.h>

namespace Sphere {
	extern void setupSphere(glm::vec3 pos, float radius);
	extern void updateSphere(glm::vec3 pos, float radius);
	extern void cleanupSphere();
}
namespace ClothMesh {
	extern void setupClothMesh();
	extern void cleanupClothMesh();
	extern void updateClothMesh(float* array_data);
	extern void drawClothMesh();

}
namespace LilSpheres {
	extern void updateParticles(int startIdx, int count, float* array_data);
	extern int particleCount;

}

#pragma region PHYSICS

glm::vec3 acceleration = { 0,-9.81,0 };
float simulationSpeed = 1;
bool simulate = true;

void Reset() {

}

#pragma endregion

class Ball {
	glm::vec3 position;
	float radius = 1;
	float mass = 1;
public:
	void Init() {
		extern bool renderSphere;
		//renderSphere = true;
		Sphere::setupSphere(position, radius);
	}
	void Update(float dt) {
		Sphere::updateSphere(position, radius);
	}
	void Cleanup() {
		Sphere::cleanupSphere();
	}
};

class Fluid {
public:

	glm::vec3  PARTICLE_START_POSITION = { 0,3,0 };
	float  PARTICLE_DISTANCE = 0.7f;

	int const RESOLUTION_X = 14;
	int const RESOLUTION_Y = 18;
	int const RESOLUTION = RESOLUTION_X * RESOLUTION_Y;


	glm::vec3* positions;
	glm::vec3* originalPositions;
	// Totes les fórmules
	std::vector<float> amplitudes;
	std::vector<float> frequencies; // ω
	std::vector <glm::vec3> waveVectors;
	std::vector <float> waveLengths;
	std::vector <float> lambdas; // λ
	std::vector <float> phases; // Φ
	float totalTime = 0;




	void SpawnParticle(int i) { 
		glm::vec3 position = PARTICLE_START_POSITION; // La inicializamos en el centro
		position.x -= (RESOLUTION_X / 2 * PARTICLE_DISTANCE) - PARTICLE_DISTANCE * .5f; // La colocamos en un punto de origen 
		position.z -= (RESOLUTION_Y / 2 * PARTICLE_DISTANCE) - PARTICLE_DISTANCE * .5f;
		position.x += i % RESOLUTION_X * PARTICLE_DISTANCE; // Setteamos su posicion original en base al offset con el tamaño del array
		position.z += ((i / RESOLUTION_X) % RESOLUTION_Y) * PARTICLE_DISTANCE; // Hacemos lo mismo pero con el offset en el otro eje
		originalPositions[i] = positions[i] = position;
		
	}

	void Init()
	{
		// Inicialitzem una fórmula per ara
		amplitudes.push_back(0.2f);
		frequencies.push_back(2.5f);
		waveLengths.push_back(2.5f);
		lambdas.push_back(1.0f);
		phases.push_back(glm::half_pi<float>()); // pi/2, per exemple
		waveVectors.push_back({ 1,0,1 }); // Només en eix X i Z
		amplitudes.push_back(0.2f);

		frequencies.push_back(2.5f);
		waveLengths.push_back(2.5f);
		lambdas.push_back(1.0f);
		phases.push_back(glm::half_pi<float>()); // pi/2, per exemple
		waveVectors.push_back({ -1,0,1 }); // Només en eix X i Z

		ClothMesh::setupClothMesh();
		positions = new glm::vec3[RESOLUTION];
		originalPositions = new glm::vec3[RESOLUTION];
		LilSpheres::particleCount = 0;
		extern bool renderCloth;
		renderCloth = true;

		for (int i = 0; i < RESOLUTION; i++)
		{
			SpawnParticle(i);
		}
		LilSpheres::particleCount = RESOLUTION;
	}

	void Update(float dt)
	{
		// actualizar cada partícula en base a las funciones (No borres)
		// http://fooplot.com/?lang=es#W3sidHlwZSI6MCwiZXEiOiJ4XjIiLCJjb2xvciI6IiMwMDAwMDAifSx7InR5cGUiOjEwMDB9XQ
		/*
			x = x0 - (k/Ḵ)Asin(k * x0 - ωt); // aixó es una posició
			y = A cos(k * x0-wt);
			Ḵ = 2π/λ
			//// 
			x es un vector, ya nos indica la X y la Z
			x = x0 - ∑((ki/Ḵi)*Ai sin(ki * x0 - ωit + Φi);
			y = ∑(Ai cos(ki * x0 - ωit + Φi);
			Φ = fase(inventado)
		*/
		for (int i = 0; i < RESOLUTION; i++)
		{

			glm::vec3 tmpVec = glm::vec3(0.0f,0.0f,0.0f);
			float tmpY = originalPositions[i].y;
			for (int j = 0; j < lambdas.size(); j++) // J = onda actual
			{
				float waveLength = glm::length(waveVectors[j]);
				// std::cout << waveLength * amplitudes[j] << std::endl; // Si la onda hace cosas raras es qüe la amplitüd es demasiado o el waveVec tambien, testear aquí
				float tmpSin = glm::dot(waveVectors[j], originalPositions[i]) - (frequencies[j] * totalTime) + phases[j];
				tmpVec += (waveVectors[j]/waveLength)  * amplitudes[j] * glm::sin(tmpSin);
				
				float tmpCos = glm::dot(waveVectors[j], originalPositions[i]) - (frequencies[j] * totalTime) + phases[j];
				tmpY += amplitudes[j] * glm::cos(tmpCos);
			}
			tmpVec = originalPositions[i] - tmpVec;
			positions[i] = {tmpVec.x, tmpY, tmpVec.z};
		}
		totalTime += dt;


		ClothMesh::updateClothMesh(&positions[0].x);
	}
	void Cleanup()
	{
		delete[] originalPositions;
		delete[] positions;
		ClothMesh::cleanupClothMesh();
	}
};

Ball ball;
Fluid fluid;
bool show_test_window = false;
void GUI() {
	bool show = true;
	ImGui::Begin("Physics Parameters", &show, 0);

	{
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::Checkbox("Run simulation", &simulate);
		if (simulate) {
			ImGui::SliderFloat("Simulation speed", &simulationSpeed, 0, 2);
		}
	}

	ImGui::End();
}

void PhysicsInit() {
	fluid.Init();
	ball.Init();
}

void PhysicsUpdate(float dt) {
	if (simulate) {
		fluid.Update(dt * simulationSpeed);
		ball.Update(dt * simulationSpeed);
	}
}

void PhysicsCleanup() {
	fluid.Cleanup();
	ball.Cleanup();
}

