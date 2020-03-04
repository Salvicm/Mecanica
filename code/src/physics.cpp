#include <imgui\imgui.h>
#include <imgui\imgui_impl_sdl_gl3.h>
#include <imgui\imgui_impl_sdl_gl3.h>
#include <glm\glm.hpp>
#include <glm\gtc\type_ptr.hpp>
#include <GL\glew.h>
#include <glm\gtc\matrix_transform.hpp>
#include <iostream>

float elasticity = 0.75f;
enum class Mode{FOUNTAIN, CASCADE};
enum class CascadeAxis{X_LEFT, X_RIGHT, Z_FRONT, Z_BACK};
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
	 // (n * pt + d)(n*pt+dt + d)
	float x = (originalPos.x * plano.x) + (originalPos.y *plano.y) + (originalPos.z * plano.z) + plano.w;
	float y = (endPos.x * plano.x) + (endPos.y *plano.y) + (endPos.z * plano.z) + plano.w;
	return x * y < 0;
}
glm::vec3 fixPos(glm::vec3 originalPos, glm::vec3 endPos, glm::vec4 plano) {
	glm::vec3 newPos = { 0,0,0 };
	glm::vec3 normalPlano = { plano.x, plano.y, plano.z };
	newPos = (endPos - (2 * (glm::dot(endPos, normalPlano) + plano.w)) * normalPlano);
	return newPos;

}
glm::vec3 fixSpeed(glm::vec3 originalSpeed, glm::vec3 endSpeed, glm::vec4 plano) { 
	glm::vec3 newSpeed = { 0,0,0 };
	glm::vec3 normalPlano = { plano.x, plano.y, plano.z };
	newSpeed = (endSpeed - (2 * (glm::dot(endSpeed, normalPlano))) * normalPlano) * elasticity;

	return newSpeed;
}

bool checkWithSphere() {
	return false;
}
struct Particles {
	Mode mode = Mode::FOUNTAIN;
	CascadeAxis axis = CascadeAxis::Z_BACK;
	float distFromAxis = 2.5f;
	float cascadeHeight = 5.0f;
	glm::vec3 fountainOrigin = { 4.5f, 9.0f,0.f }; // TODO Modificar el centro en la UI // X y Z deben ir entre 5 y -5, la Y entre 0 y 1
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
			float tmpX = (float)(rand() % 500) / 100.f - 2.5f;
			float tmpZ = (float)(rand() % 500) / 100.f - 2.5f;
			float tmpY = 5.f;
			primaSpeeds[i] = speeds[i] = { tmpX, tmpY, tmpZ };
			lifeTime[i] = (((float)(rand() % 100) / 100.f) * 2.f) + 2;
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
					originPosition = {-5.f + distFromAxis, cascadeHeight, (((float)(rand() % 100) / 100.f) * 5.f) -10 };
					break;
				case CascadeAxis::X_RIGHT:
					originPosition = {+5.f - distFromAxis, cascadeHeight, (((float)(rand() % 100) / 100.f) * 5.f) -10 };
					break;
				case CascadeAxis::Z_FRONT:
					originPosition = { (((float)(rand() % 100) / 100.f) * 5.f) - 10 , cascadeHeight, +5.f - distFromAxis }; 
					break;
				case CascadeAxis::Z_BACK:
					originPosition = { (((float)(rand() % 100) / 100.f) * 5.f) - 10 , cascadeHeight, -5.f + distFromAxis }; 
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
		glm::vec4 plano; 
		for (int i = 0; i < maxParticles; i++)
		{
			bool collided = false;
			primaPositions[i] = eulerSolver(positions[i], speeds[i], dt);
			primaSpeeds[i] = eulerSolver(speeds[i], acceleration, dt);
			currentLifeTime[i] += dt;
			if (currentLifeTime[i] >= lifeTime[i]) {
				float x = -5 + min + (float)rand() / (RAND_MAX / (max - min));
				float z = -5 + min + (float)rand() / (RAND_MAX / (max - min));
				float tmpX = (float)(rand() % 500) / 100.f - 2.5f;
				float tmpZ = (float)(rand() % 500) / 100.f - 2.5f;
				float tmpY = 5.f;
				primaSpeeds[i] = speeds[i] = { tmpX, tmpY, tmpZ };
				currentLifeTime[i] = 0;
				switch (mode)
				{
				case Mode::FOUNTAIN:
					primaPositions[i] = positions[i] = fountainOrigin;
					break;
				case Mode::CASCADE:
					break;
				default:
					break;
				}
				lifeTime[i] = (((float)(rand() % 100) / 100.f) * 2.f) + 2;

			}
			
			plano = getRectFormula(
				// Basandonos en los indices del cubo 
				{ Box::cubeVerts[4 * 3], Box::cubeVerts[4 * 3 + 1], Box::cubeVerts[4 * 3 + 2] },
				{ Box::cubeVerts[5 * 3], Box::cubeVerts[5 * 3 + 1], Box::cubeVerts[5 * 3 + 2] },
				{ Box::cubeVerts[6 * 3], Box::cubeVerts[6 * 3 + 1], Box::cubeVerts[6 * 3 + 2] },
				{ Box::cubeVerts[7 * 3], Box::cubeVerts[7 * 3 + 1], Box::cubeVerts[7 * 3 + 2] });
			if (checkWithPlane(positions[i], primaPositions[i], plano)) {
				collided = true;
				primaPositions[i] = fixPos(positions[i], primaPositions[i], plano);
				primaSpeeds[i] = fixSpeed(speeds[i], primaSpeeds[i], plano);
			}

			for (int j = 0; j < 20; j+=4) {
				plano = getRectFormula(
					// Basandonos en los indices del cubo 
					{ Box::cubeVerts[Box::cubeIdx[j] * 3], Box::cubeVerts[Box::cubeIdx[j] * 3 + 1], Box::cubeVerts[Box::cubeIdx[j] * 3 + 2] },
					{ Box::cubeVerts[Box::cubeIdx[j+1] * 3], Box::cubeVerts[Box::cubeIdx[j+1] * 3 + 1], Box::cubeVerts[Box::cubeIdx[j+1] * 3 + 2] },
					{ Box::cubeVerts[Box::cubeIdx[j+2] * 3], Box::cubeVerts[Box::cubeIdx[j + 2] * 3 + 1], Box::cubeVerts[Box::cubeIdx[j + 2] * 3 + 2] },
					{ Box::cubeVerts[Box::cubeIdx[j+3] * 3], Box::cubeVerts[Box::cubeIdx[j + 3] * 3 + 1], Box::cubeVerts[Box::cubeIdx[j + 3] * 3 + 2] });
				if (checkWithPlane(positions[i], primaPositions[i], plano)) {
					collided = true;
					primaPositions[i] = fixPos(positions[i], primaPositions[i], plano);
					primaSpeeds[i] = fixSpeed(speeds[i], primaSpeeds[i], plano);
				}
			}
			speeds[i] = primaSpeeds[i];
			positions[i] = primaPositions[i];
			
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
