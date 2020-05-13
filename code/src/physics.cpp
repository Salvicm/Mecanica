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
#include <cmath>
#if __has_include("FastNoise.h")
#  include "FastNoise.h"
#  define PERLIN 1
#else
#  define PERLIN 0
#endif

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

struct Wave {
	float amplitude;
	float frequency;// ω
	glm::vec3 vector;
	float length;
	float lambda;// λ
	float phase;// Φ
	Wave() {
		amplitude = .2f;
		frequency = 2.5f;
		vector = { 1,0,1 };
		length = 2.5f;
		lambda = 1.f;
		phase = glm::half_pi<float>();
	}
};

#if PERLIN
struct Noise {
	FastNoise noise;
	float frequency = 1;
	int octaves = 8;
	glm::vec3 speed = { 1,0,1 };
	glm::vec3 influence = { .1f,.1f,.1f };
	float scale = 1;
	glm::vec3 position = { 0,0,0 };
	void NextFrame(float dt) {
		position += speed * dt;
	}
	float GetValue(glm::vec3 pos, float range = 1) {
		return noise.GetNoise((position.x + pos.x) * scale, (position.y + pos.y) * scale, (position.z + pos.z) * scale) * (range * 2) - range;
	}
};
#endif

struct WaveType {
	std::string name = "Custom";
	std::vector<Wave> waves;
	WaveType() {
		Wave tempWave;
		tempWave.vector = { -1,0,1 };
		waves.push_back(tempWave);
		tempWave.vector = { 1,0,1 };
		waves.push_back(tempWave);
	}
#if PERLIN
	glm::vec3 speed = { 0,0,0 };
	glm::vec3 influence = { 0,0,0 };
	float scale = 0;
#endif
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
	std::vector<WaveType> waveTypes;
	std::vector<Wave> waves;
#if PERLIN
	Noise rand;
#endif

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
		waveTypes.push_back(WaveType());
		SetWaveType(WaveType());

#if PERLIN
		rand.noise.SetNoiseType(FastNoise::NoiseType::Value);
		rand.noise.SetFrequency(rand.frequency);
		rand.noise.SetInterp(FastNoise::Interp::Quintic);
#endif

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
#if PERLIN
		rand.NextFrame(dt);
#endif
		for (int i = 0; i < RESOLUTION; i++)
		{

			glm::vec3 tmpVec = glm::vec3(0.0f, 0.0f, 0.0f);
			float tmpY = originalPositions[i].y;
			for (int j = 0; j < waves.size(); j++) // J = onda actual
			{

				//std::cout << randomFactor << std::endl;

				// std::cout << waveLength * amplitudes[j] << std::endl; // Si la onda hace cosas raras es qüe la amplitüd es demasiado o el waveVec tambien, testear aquí
				float tmpSin = glm::dot(waves[j].vector, originalPositions[i]) - (waves[j].frequency * totalTime) + waves[j].phase;
				tmpVec += ((waves[j].vector / glm::length(waves[j].length)) * waves[j].amplitude * glm::sin(tmpSin));

				float tmpCos = glm::dot(waves[j].vector, originalPositions[i]) - (waves[j].frequency * totalTime) + waves[j].phase;
				tmpY += (waves[j].amplitude * glm::cos(tmpCos));
			}
			tmpVec = originalPositions[i] - tmpVec;
			glm::vec3 newPos = { tmpVec.x, tmpY, tmpVec.z };
#if PERLIN
			if (rand.influence.x > 0) {
				newPos.x += rand.GetValue(newPos, rand.influence.x);
			}
			if (rand.influence.y > 0) {
				newPos.y += rand.GetValue(newPos, rand.influence.y);
			}
			if (rand.influence.z > 0) {
				newPos.z += rand.GetValue(newPos, rand.influence.z);
			}
#endif
			positions[i] = newPos;
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

	void SetWaveType(WaveType wt) {
		waves = wt.waves;
#if PERLIN
		rand.speed = wt.speed;
		rand.influence = wt.influence;
		rand.scale = wt.scale;
#endif
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

		ImGui::Spacing();
		ImGui::Text("Fluid:");
		ImGui::Spacing();
#if PERLIN
		ImGui::DragFloat3("Noise influence", &fluid.rand.influence[0], .01f);
		ImGui::DragFloat3("Noise speed", &fluid.rand.speed[0], .01f);
		ImGui::DragFloat("Noise scale", &fluid.rand.scale, .01f);
		ImGui::Spacing();
#endif
		int newWaveCount = fluid.waves.size();
		ImGui::DragInt("Wave count", &newWaveCount, .1f);
		for (size_t i = 0; i < fluid.waves.size(); i++)
		{
			ImGui::Spacing();
			std::string tempString = std::to_string(i + 1);
			tempString += " Amplitude";
			ImGui::DragFloat(tempString.c_str(), &fluid.waves[i].amplitude, .01f);
			tempString = std::to_string(i + 1);
			tempString += " Frequency";
			ImGui::DragFloat(tempString.c_str(), &fluid.waves[i].frequency, .01f);
			tempString = std::to_string(i + 1);
			tempString += " Lambda";
			ImGui::DragFloat(tempString.c_str(), &fluid.waves[i].lambda, .01f);
			tempString = std::to_string(i + 1);
			tempString += " Length";
			ImGui::DragFloat(tempString.c_str(), &fluid.waves[i].length, .01f);
			tempString = std::to_string(i + 1);
			tempString += " Phase";
			ImGui::DragFloat(tempString.c_str(), &fluid.waves[i].phase, .01f);
			tempString = std::to_string(i + 1);
			tempString += " Vector";
			ImGui::DragFloat3(tempString.c_str(), &fluid.waves[i].vector[0], .01f);
		}
		if (newWaveCount < 0)
			newWaveCount = 0;
		if (newWaveCount != fluid.waves.size()) {
			while (newWaveCount > fluid.waves.size()) {
				fluid.waves.push_back(Wave());
			}
			while (newWaveCount < fluid.waves.size()) {
				fluid.waves.pop_back();
			}
		}
	}

	ImGui::End();
}

void PhysicsInit() {
	fluid.Init();
	ball.Init();
}

void PhysicsUpdate(float dt) {
	srand(time(NULL));
	if (simulate) {
		fluid.Update(dt * simulationSpeed);
		ball.Update(dt * simulationSpeed);
	}
}

void PhysicsCleanup() {
	fluid.Cleanup();
	ball.Cleanup();
}

