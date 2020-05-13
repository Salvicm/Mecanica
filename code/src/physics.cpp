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
		renderSphere = true;
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

	glm::vec3  PARTICLE_START_POSITION = { 0,9,0 };
	float  PARTICLE_DISTANCE = .3f;

	int const RESOLUTION_X = 14;
	int const RESOLUTION_Y = 18;
	int const RESOLUTION = RESOLUTION_X * RESOLUTION_Y;


	glm::vec3* positions;




	void SpawnParticle(int i) {
		glm::vec3 position = PARTICLE_START_POSITION; // La inicializamos en el centro
		position.x -= (RESOLUTION_X / 2 * PARTICLE_DISTANCE) - PARTICLE_DISTANCE * .5f; // La colocamos en un punto de origen 
		position.z -= (RESOLUTION_Y / 2 * PARTICLE_DISTANCE) - PARTICLE_DISTANCE * .5f;
		position.x += i % RESOLUTION_X * PARTICLE_DISTANCE; // Setteamos su posicion original en base al offset con el tamaño del array
		position.z += ((i / RESOLUTION_X) % RESOLUTION_Y) * PARTICLE_DISTANCE; // Hacemos lo mismo pero con el offset en el otro eje
		positions[i] = position;
	}

	void Init()
	{
		ClothMesh::setupClothMesh();
		positions = new glm::vec3[RESOLUTION];
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
		ClothMesh::updateClothMesh(&positions[0].x);
	}
	void Cleanup()
	{
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

	}

	ImGui::End();
}

void PhysicsInit() {
	fluid.Init();
	ball.Init();
}

void PhysicsUpdate(float dt) {
	fluid.Update(dt);
	ball.Update(dt);
}

void PhysicsCleanup() {
	fluid.Cleanup();
	ball.Cleanup();
}

