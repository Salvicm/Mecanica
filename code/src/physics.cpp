#include <imgui\imgui.h>
#include <imgui\imgui_impl_sdl_gl3.h>
#include <imgui\imgui_impl_sdl_gl3.h>
#include <glm\glm.hpp>
#include <glm\gtc\matrix_transform.hpp>
#include <vector>
#include <iostream>

namespace LilSpheres {
	extern void updateParticles(int startIdx, int count, float* array_data);
}

struct Particles {

	int maxParticles = 100;
	std::vector<float> positions;
	int PartCount;
	// Physics parameters
	float min = 0.0f;
	float max = 10.0f;

	void SetMaxParticles(int n) {
		maxParticles = n;
	}
	void InitParticles() {
		positions.clear();
		for (int i = 0; i < maxParticles * 3; i++)
		{
			positions.push_back(0.0);
		}
	}
	void UpdateParticles(float dt) {
		PartCount = positions.size();
		for (int i = 0; i < PartCount; i += 3)
		{
			float x = -5 + min + (float)rand() / (RAND_MAX / (max - min));
			float y = min + (float)rand() / (RAND_MAX / (max - min));
			float z = -5 + min + (float)rand() / (RAND_MAX / (max - min));
			std::cout << "Creating particle at: " << x << ".2f, " << y << ".2f, " << z << ".2f\n";
			positions[i] = x;
			positions[i + 1] = y;
			positions[i + 2] = z;
		}

		LilSpheres::updateParticles(0, PartCount, &positions[0]);

	}
	void CleanParticles() {
		positions.clear();
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

void particleContainer(){/*

	s_PS.numParticles = 100;
	s_PS.position = new glm::vec3[s_PS.numParticles];

	extern bool renderParticles; renderParticles = true
	LilSpheres::firstParticleIdx = 0;
	LilSpheres::particleCount = s_PS.numParticles;*/

}


