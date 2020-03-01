#include <imgui\imgui.h>
#include <imgui\imgui_impl_sdl_gl3.h>
#include <imgui\imgui_impl_sdl_gl3.h>
#include <glm\glm.hpp>
#include <glm\gtc\matrix_transform.hpp>
#include <vector>
#include <iostream>

namespace LilSpheres {
	extern void updateParticles(int startIdx, int count, float* array_data);
	extern int particleCount;

}
glm::vec3 eulerSolver(glm::vec3 origin, glm::vec3 end, float _dt) {
	return origin + _dt * end;
}
glm::vec3 eulerFixer(glm::vec3 origin, glm::vec3 end, glm::vec4 planeInfo) {
	return{ 0,0,0 };
}
bool checkWithPlane() {
	return false;
}
bool checkWithSphere() {
	return false;
}
struct Particles {
	glm::vec3 acceleration = { 0, -9.81f, 0 };
	int maxParticles = 100;
	glm::vec3 *positions;
	glm::vec3 *speeds;
	float *lifeTime;
	float *currentLifeTime;
	glm::vec3 originalSpeed = { 0,1,0 };
	float originalLifetime = 1.5f;
	// Physics parameters
	float min = 0.0f;
	float max = 10.0f;

	void SetMaxParticles(int n) {
		maxParticles = n;
	}
	void InitParticles() {
		positions = new glm::vec3[maxParticles];
		speeds = new glm::vec3[maxParticles];
		lifeTime = new float[maxParticles];
		currentLifeTime = new float[maxParticles];
		LilSpheres::particleCount = maxParticles;
		extern bool renderParticles;
		renderParticles = true;
		for (int i = 0; i < maxParticles; i++)
		{
			float x = -5 + min + (float)rand() / (RAND_MAX / (max - min));
			float z = -5 + min + (float)rand() / (RAND_MAX / (max - min));
			speeds[i] = originalSpeed;
			lifeTime[i] = (float(rand() % 100) +1)/ 100 + 1 ;
			currentLifeTime[i] = 0;
			positions[i] = { x,max,z };
		}
	}
	void UpdateParticles(float dt) {

		for (int i = 0; i < maxParticles; i++)
		{
			positions[i] = eulerSolver(positions[i], speeds[i], dt);
			speeds[i] = eulerSolver(speeds[i], acceleration, dt);
			currentLifeTime[i] += dt;
			if (currentLifeTime[i] >= lifeTime[i]) {
				float x = -5 + min + (float)rand() / (RAND_MAX / (max - min));
				float z = -5 + min + (float)rand() / (RAND_MAX / (max - min));
				speeds[i] = originalSpeed;
				currentLifeTime[i] = 0;
				positions[i] = { x,max,z };
				lifeTime[i] = (float(rand() % 100) + 1) / 100 + 1;

			}
			if (checkWithPlane()) {
				std::cout << "Particle nº: [" << i << "] Collided with a plane" << std::endl;
			}
			if (checkWithSphere()) {
				std::cout << "Particle nº: [" << i << "] Collided with a sphere" << std::endl;
			}
		}
		// Check collision
		LilSpheres::updateParticles(0, maxParticles, &positions[0].x);

	}
	void CleanParticles() {
		delete[] positions;
		delete[] speeds;
		delete[] lifeTime;
		delete[] currentLifeTime;

	}
} parts;
extern void Exemple_GUI();
extern void Exemple_PhysicsInit();
extern void Exemple_PhysicsUpdate(float dt);
extern void Exemple_PhysicsCleanup();
bool show_test_window = false;
void GUI() {
	bool show = true;
	ImGui::Begin("Physics Parameters", &show, 0);

	{
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);//FrameRate
		Exemple_GUI();
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

