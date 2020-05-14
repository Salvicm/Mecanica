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

glm::vec3 acceleration = { 0,-0.981f,0 };
float simulationSpeed = 1;
bool simulate = true;
float totalTime = 0;

struct Line {
	glm::vec3 point;
	glm::vec3 direction;
};
float LinePlaneCollisionRange(const Line& line, const glm::vec4& plane) {
	glm::vec3 normal = plane;
	float dotPoint = glm::dot(normal, line.point);
	float dotDirection = glm::dot(normal, line.direction);
	return ((plane.w - dotPoint) / dotDirection);
}
glm::vec3 LinePoint(const Line& line, float range) {
	return line.point + range * line.direction;
}
glm::vec3 LinePlaneCollision(const Line& line, const glm::vec4& plane) {
	return LinePoint(line, LinePlaneCollisionRange(line, plane));
}

//glm::vec4 GetPlaneUp(const glm::vec3& A, const glm::vec3& B, const glm::vec3& C) {
//	glm::vec4 plane = { 0,1,0, 5 };
//
//	float a1 = B.x - A.x;
//	float b1 = B.y - A.y;
//	float c1 = B.z - A.z;
//	float a2 = C.x - A.x;
//	float b2 = C.y - A.y;
//	float c2 = C.z - A.z;
//	float a = b1 * c2 - b2 * c1;
//	float b = a2 * c1 - a1 * c2;
//	float c = a1 * b2 - b1 * a2;
//	float d = (-a * A.x - b * A.y - c * A.z);
//	plane.x = a;
//	plane.y = b;
//	plane.z = c;
//	plane.w = d;
//	return plane;
//}
glm::vec4 GetPlaneUpGLM(const glm::vec3& A, const glm::vec3& B, const glm::vec3& C) {
	glm::vec4 plane = { 0,1,0, 5 };
	glm::vec3 AB = B - A;
	glm::vec3 AC = C - A;
	glm::vec3 normal = glm::cross(AB, AC);
	normal = glm::normalize(normal);
	if (glm::dot(normal, glm::vec3(0, 1, 0)) < 0) {
		normal *= -1;
	}

	float d = (-normal.x * A.x - normal.y * A.y - normal.z * A.z);
	plane.x = normal.x;
	plane.y = normal.y;
	plane.z = normal.z;
	plane.w = d;
	return plane;
}

#pragma endregion


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
	void Random() {
		amplitude = ((static_cast <float> (rand()) * 2 / static_cast <float> (RAND_MAX)) - 1);
		frequency = ((static_cast <float> (rand()) * 2 / static_cast <float> (RAND_MAX)) - 1) * 5;
		vector = { ((static_cast <float> (rand()) * 2 / static_cast <float> (RAND_MAX)) - 1),
		((static_cast <float> (rand()) * 2 / static_cast <float> (RAND_MAX)) - 1),
		((static_cast <float> (rand()) * 2 / static_cast <float> (RAND_MAX)) - 1) };
		length = ((static_cast <float> (rand()) * 2 / static_cast <float> (RAND_MAX)) - 1) * 3;
		lambda = ((static_cast <float> (rand()) * 2 / static_cast <float> (RAND_MAX)) - 1);
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
	std::vector<Wave> waves;
	WaveType() {
		Wave tempWave;
		tempWave.vector = { -1,0,1 };
		waves.push_back(tempWave);
		tempWave.vector = { 1,0,1 };
		waves.push_back(tempWave);
	}
	glm::vec3 speed = { 0,0,0 };
	glm::vec3 influence = { 0,0,0 };
	float scale = 0;
};
const int waveTypesMax = 3;
WaveType waveTypes[waveTypesMax];
class Fluid {
public:

	glm::vec3  PARTICLE_START_POSITION = { 0,3,0 };
	float  PARTICLE_DISTANCE = 0.7f;
	float density = 1;

	int const RESOLUTION_X = 14;
	int const RESOLUTION_Y = 18;
	int const RESOLUTION = RESOLUTION_X * RESOLUTION_Y;


	glm::vec3* positions;
	glm::vec3* originalPositions;
	// Totes les fórmules
	int selectedWave;
	std::vector<Wave> waves;
#if PERLIN
	Noise rand;
#endif





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
		selectedWave = 0;
		SetWaveType(waveTypes[0]);
		{
			WaveType tempType;
			tempType.waves.clear();
			Wave tempWave;
			tempWave.amplitude = .7f;
			tempWave.frequency = 3;
			tempWave.lambda = 0;
			tempWave.length = .72;
			tempWave.vector = { .2f, 0, -1 };
			tempType.waves.push_back(tempWave);
			tempType.waves.push_back(Wave());
			tempType.influence = { .2f,.1f,.2f };
			tempType.speed = { 1,0,5 };
			tempType.scale = .5f;
			waveTypes[1] = tempType;
		}
		{
			WaveType tempType;
			tempType.waves.clear();
			Wave tempWave;
			tempWave.amplitude = .05f;
			tempType.waves.push_back(tempWave);
			tempWave.frequency = 2;
			tempWave.vector = {1,0,-1};
			tempType.waves.push_back(tempWave);
			tempWave.frequency = 1;
			tempWave.vector = {-1,0,-1};
			tempType.waves.push_back(tempWave);
			tempType.influence = { .2f,.1f,.2f };
			tempType.speed = { 1,0,5 };
			tempType.scale = .5f;
			waveTypes[2] = tempType;
		}
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

class Ball {
	glm::vec3 position;
	glm::vec3 spawnPosition = { 0,5,0 };
	glm::vec3 linearMomentum = { 0.0f,0.0f,0.0f };
	glm::vec3 force = { 0,0,0 };
	float nextTime = 0;
public:
	bool simulate = true;
	bool simulateDirection = false;
	float radius = 1;
	float mass = 1;
	float resetTime = 15.f;
	void Init() {
		extern bool renderSphere;
		renderSphere = true;

		extern bool renderParticles;
		renderParticles = true;
		LilSpheres::particleCount = 3;

		Spawn();
		Sphere::setupSphere(position, radius);
	}
	void Spawn() {
		linearMomentum = { 0.0f,0.0f,0.0f };
		force = { 0,0,0 };
		position = spawnPosition;
		position.x += ((static_cast <float> (rand()) * 2 / static_cast <float> (RAND_MAX)) - 1) * 2.5f;
		position.y += ((static_cast <float> (rand()) * 2 / static_cast <float> (RAND_MAX)) - 1) * 2.5f;
		position.z += ((static_cast <float> (rand()) * 2 / static_cast <float> (RAND_MAX)) - 1) * 2.5f;
		nextTime = totalTime + resetTime;
	}
	void Update(float dt, const Fluid& fluid) {
		if (nextTime < totalTime) {
			Spawn();
		}
		force = { 0,0,0 };
		CalculateDisplacement(getNearestPlane(fluid), fluid.density);
		SemiImplicitEuler(dt);
		Sphere::updateSphere(position, radius);
	}

	glm::vec4 getNearestPlane(const Fluid& fluid) {
		glm::vec3 points[3];
		points[0] = points[1] = points[2] = fluid.positions[0];
		glm::vec3 pos = position;
		//pos -= glm::vec3(0, 1, 0) * radius;
		pos.y = 0;
		for (size_t i = 1; i < fluid.RESOLUTION; i++)
		{
			if (glm::distance(fluid.positions[i], glm::vec3(pos.x, fluid.positions[i].y, pos.z)) < glm::distance(points[0], glm::vec3(pos.x, points[0].y, pos.z))) {
				points[2] = points[1];
				points[1] = points[0];
				points[0] = fluid.positions[i];
			}
		}
		LilSpheres::updateParticles(0, 3, &points[0].x);
		//std::cout << "P: " << position.x << ", " << position.y << ", " << position.z << std::endl;
		//std::cout << "A: " << points[0].x << ", " << points[0].y << ", " << points[0].z << std::endl;
		//std::cout << "B: " << points[1].x << ", " << points[1].y << ", " << points[1].z << std::endl;
		//std::cout << "C: " << points[2].x << ", " << points[2].y << ", " << points[2].z << std::endl;
		//glm::vec4 plane = GetPlaneUp(points[0], points[1], points[2]);
		//glm::vec4 planeGLM = GetPlaneUpGLM(points[0], points[1], points[2]);
		return GetPlaneUpGLM(points[0], points[1], points[2]);
	}

	void CalculateDisplacement(const glm::vec4& plane, float fluidDensity) {
		glm::vec3 planeNormal = plane;
		planeNormal = glm::normalize(planeNormal);
		float sliceValue = LinePlaneCollisionRange({ position, planeNormal }, plane);
		glm::vec3 slicePoint = LinePoint({ position, planeNormal }, sliceValue);
		if (sliceValue > 0) {
			float volumeDisplaced = 0;
			if (sliceValue > radius* radius) {
				planeNormal = { 0,1,0 };
				volumeDisplaced = (4 / 3) * glm::pi<float>() * radius * radius * radius;
			}
			else if (sliceValue > radius) {
				sliceValue = fmod(sliceValue, radius);
				float notDisplaced = (glm::pi<float>() * sliceValue * sliceValue) / 3;
				notDisplaced *= ((3 * radius) - sliceValue);
				float sphereVolume = (4 / 3) * glm::pi<float>() * radius * radius * radius;
				volumeDisplaced = sphereVolume - notDisplaced;
			}
			else {
				volumeDisplaced = (glm::pi<float>() * sliceValue * sliceValue) / 3;
				volumeDisplaced *= ((3 * radius) - sliceValue);
			}
			if (!simulateDirection) {
				planeNormal = { 0,-1,0 };
				force += fluidDensity * volumeDisplaced * acceleration * planeNormal;
			}
			else {
				force += fluidDensity * volumeDisplaced * planeNormal * glm::length(acceleration);
			}
		}
	}

	void SemiImplicitEuler(const float _dt) {
		linearMomentum += (acceleration + force);
		position += _dt * (linearMomentum / mass);
	}

	void Cleanup() {
		Sphere::cleanupSphere();
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
			ImGui::DragFloat3("Acceleration", &acceleration.x, .01f);
		}
		ImGui::Spacing();
		ImGui::Checkbox("Sphere", &ball.simulate);
		if (ball.simulate) {
			ImGui::DragFloat("Radius", &ball.radius, .01f);
			ImGui::DragFloat("Mass", &ball.mass, .01f);
			ImGui::Checkbox("Emulate direction", &ball.simulateDirection);
			if (ImGui::Button("Reset")) {
				ball.Spawn();
			}
		}

		ImGui::Spacing();
		ImGui::Text("Fluid:");
		ImGui::SliderFloat("Density", &fluid.density, 0, 10);
		{
			int newPreset = fluid.selectedWave;
			ImGui::Combo("Wave type", &newPreset, "Custom\0Sea\0Lake");
			if (newPreset != fluid.selectedWave) {
				fluid.SetWaveType(waveTypes[newPreset]);
				fluid.selectedWave = newPreset;
			}
		}
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
			Wave temp;
			while (newWaveCount > fluid.waves.size()) {
				temp.Random();
				fluid.waves.push_back(temp);
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
		if (ball.simulate)
			ball.Update(dt * simulationSpeed, fluid);
		totalTime += dt * simulationSpeed;
	}
}

void PhysicsCleanup() {
	fluid.Cleanup();
	ball.Cleanup();
}

