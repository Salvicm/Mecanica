#include <imgui\imgui.h>
#include <imgui\imgui_impl_sdl_gl3.h>
#include <imgui\imgui_impl_sdl_gl3.h>
#include <glm\glm.hpp>
#include <glm\gtc\type_ptr.hpp>
#include <GL\glew.h>
#include <glm\gtc\matrix_transform.hpp>
#include <iostream>

//Function declarations
glm::vec4 getRectFormula(glm::vec3 _a, glm::vec3 _b, glm::vec3 _c, glm::vec3 _d);

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
glm::vec3 eulerSolver(glm::vec3 origin, glm::vec3 end, float _dt) {
	return origin + _dt * end;
}
glm::vec3 eulerFixer(glm::vec3 origin, glm::vec3 end, glm::vec4 planeInfo) {
	return{ 0,0,0 };
}
bool checkWithPlane(glm::vec3 originalPos, glm::vec3 endPos, glm::vec4 plano) {
	return false;
}
bool checkWithSphere() {
	return false;
}
struct Particles {
	glm::vec3 acceleration = { 0, -9.81f, 0 };
	int maxParticles = 100;
	glm::vec3 *positions;
	glm::vec3 *primaPositions;
	glm::vec3 *speeds;
	glm::vec3 *primaSpeeds;
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
		primaPositions = new glm::vec3[maxParticles];
		primaSpeeds = new glm::vec3[maxParticles];

		lifeTime = new float[maxParticles];
		currentLifeTime = new float[maxParticles];
		LilSpheres::particleCount = maxParticles;
		extern bool renderParticles;
		renderParticles = true;
		for (int i = 0; i < maxParticles; i++)
		{
			float x = -5 + min + (float)rand() / (RAND_MAX / (max - min));
			float z = -5 + min + (float)rand() / (RAND_MAX / (max - min));
			primaSpeeds[i] = speeds[i] = originalSpeed;
			lifeTime[i] = (float(rand() % 100) +1)/ 100 + 1 ;
			currentLifeTime[i] = 0;
			primaPositions[i] = positions[i] = { x,max,z };
		}
	}
	void UpdateParticles(float dt) {

		for (int i = 0; i < maxParticles; i++)
		{
			primaPositions[i] = eulerSolver(positions[i], speeds[i], dt);
			primaSpeeds[i] = eulerSolver(speeds[i], acceleration, dt);
			currentLifeTime[i] += dt;
			if (currentLifeTime[i] >= lifeTime[i]) {
				float x = -5 + min + (float)rand() / (RAND_MAX / (max - min));
				float z = -5 + min + (float)rand() / (RAND_MAX / (max - min));

				primaSpeeds[i] = speeds[i] = originalSpeed;
				currentLifeTime[i] = 0;
				primaPositions[i] = positions[i] = { x,max,z };
				lifeTime[i] = (float(rand() % 100) + 1) / 100 + 1;

			}
			
			
			if (checkWithPlane(positions[i], primaPositions[i], getRectFormula(
				// Basandonos en los indices del cubo 
				{ Box::cubeVerts[Box::cubeIdx[0] * 3], Box::cubeVerts[Box::cubeIdx[0] * 3 + 1], Box::cubeVerts[Box::cubeIdx[0] * 3 + 2]},
				{ Box::cubeVerts[Box::cubeIdx[1] * 3], Box::cubeVerts[Box::cubeIdx[1] * 3 + 1], Box::cubeVerts[Box::cubeIdx[1] * 3 + 2] },
				{ Box::cubeVerts[Box::cubeIdx[2] * 3], Box::cubeVerts[Box::cubeIdx[2] * 3 + 1], Box::cubeVerts[Box::cubeIdx[2] * 3 + 2] },
				{ Box::cubeVerts[Box::cubeIdx[3] * 3], Box::cubeVerts[Box::cubeIdx[3] * 3 + 1], Box::cubeVerts[Box::cubeIdx[3] * 3 + 2] }))) {
				std::cout << "Particle nº: [" << i << "] Collided with a plane" << std::endl;
			} else if (checkWithPlane(positions[i], primaPositions[i], getRectFormula(
				// Basandonos en los indices del cubo 
				{ Box::cubeVerts[Box::cubeIdx[4] * 3], Box::cubeVerts[Box::cubeIdx[4] * 3 + 1], Box::cubeVerts[Box::cubeIdx[4] * 3 + 2] },
				{ Box::cubeVerts[Box::cubeIdx[5] * 3], Box::cubeVerts[Box::cubeIdx[5] * 3 + 1], Box::cubeVerts[Box::cubeIdx[5] * 3 + 2] },
				{ Box::cubeVerts[Box::cubeIdx[6] * 3], Box::cubeVerts[Box::cubeIdx[6] * 3 + 1], Box::cubeVerts[Box::cubeIdx[6] * 3 + 2] },
				{ Box::cubeVerts[Box::cubeIdx[7] * 3], Box::cubeVerts[Box::cubeIdx[7] * 3 + 1], Box::cubeVerts[Box::cubeIdx[7] * 3 + 2] }))) {
				std::cout << "Particle nº: [" << i << "] Collided with a plane" << std::endl;
			}else if (checkWithPlane(positions[i], primaPositions[i], getRectFormula(
				// Basandonos en los indices del cubo 
				{ Box::cubeVerts[Box::cubeIdx[8] * 3], Box::cubeVerts[Box::cubeIdx[8] * 3 + 1], Box::cubeVerts[Box::cubeIdx[8] * 3 + 2] },
				{ Box::cubeVerts[Box::cubeIdx[9] * 3], Box::cubeVerts[Box::cubeIdx[9] * 3 + 1], Box::cubeVerts[Box::cubeIdx[9] * 3 + 2] },
				{ Box::cubeVerts[Box::cubeIdx[10] * 3], Box::cubeVerts[Box::cubeIdx[10] * 3 + 1], Box::cubeVerts[Box::cubeIdx[10] * 3 + 2] },
				{ Box::cubeVerts[Box::cubeIdx[11] * 3], Box::cubeVerts[Box::cubeIdx[11] * 3 + 1], Box::cubeVerts[Box::cubeIdx[11] * 3 + 2] }))) {
				std::cout << "Particle nº: [" << i << "] Collided with a plane" << std::endl;
			} else if (checkWithPlane(positions[i], primaPositions[i], getRectFormula(
				// Basandonos en los indices del cubo 
				{ Box::cubeVerts[Box::cubeIdx[12] * 3], Box::cubeVerts[Box::cubeIdx[12] * 3 + 1], Box::cubeVerts[Box::cubeIdx[12] * 3 + 2] },
				{ Box::cubeVerts[Box::cubeIdx[13] * 3], Box::cubeVerts[Box::cubeIdx[13] * 3 + 1], Box::cubeVerts[Box::cubeIdx[13] * 3 + 2] },
				{ Box::cubeVerts[Box::cubeIdx[14] * 3], Box::cubeVerts[Box::cubeIdx[14] * 3 + 1], Box::cubeVerts[Box::cubeIdx[14] * 3 + 2] },
				{ Box::cubeVerts[Box::cubeIdx[15] * 3], Box::cubeVerts[Box::cubeIdx[15] * 3 + 1], Box::cubeVerts[Box::cubeIdx[15] * 3 + 2] }))) {
				std::cout << "Particle nº: [" << i << "] Collided with a plane" << std::endl;
			} else if (checkWithPlane(positions[i], primaPositions[i], getRectFormula(
				// Basandonos en los indices del cubo 
				{ Box::cubeVerts[Box::cubeIdx[16] * 3], Box::cubeVerts[Box::cubeIdx[16] * 3 + 1], Box::cubeVerts[Box::cubeIdx[16] * 3 + 2] },
				{ Box::cubeVerts[Box::cubeIdx[17] * 3], Box::cubeVerts[Box::cubeIdx[17] * 3 + 1], Box::cubeVerts[Box::cubeIdx[17] * 3 + 2] },
				{ Box::cubeVerts[Box::cubeIdx[18] * 3], Box::cubeVerts[Box::cubeIdx[18] * 3 + 1], Box::cubeVerts[Box::cubeIdx[18] * 3 + 2] },
				{ Box::cubeVerts[Box::cubeIdx[19] * 3], Box::cubeVerts[Box::cubeIdx[19] * 3 + 1], Box::cubeVerts[Box::cubeIdx[19] * 3 + 2] }))) {
				std::cout << "Particle nº: [" << i << "] Collided with a plane" << std::endl;
			}



			/*
			else if (checkWithSphere()) {
				std::cout << "Particle nº: [" << i << "] Collided with a sphere" << std::endl;
			}*/
			else {
				speeds[i] = primaSpeeds[i];
				positions[i] = primaPositions[i];
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
		delete[] primaPositions;
		delete[] primaSpeeds;
	}
} parts;


bool show_test_window = false;
void GUI() {
	bool show = true;
	ImGui::Begin("Physics Parameters", &show, 0);

	{
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);//FrameRate
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
