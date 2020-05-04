#include <imgui\imgui.h>
#include <imgui\imgui_impl_sdl_gl3.h>

#include <glm\glm.hpp>
#include <glm\gtc\type_ptr.hpp>
#include <GL\glew.h>
#include <glm\gtc\matrix_transform.hpp>
#include <glm/gtx/closest_point.hpp>
#include <iostream>
#include <string>
#include <vector>

//Exemple
extern void Exemple_GUI();
extern void Exemple_PhysicsInit();
extern void Exemple_PhysicsUpdate(float dt);
extern void Exemple_PhysicsCleanup();

namespace Box {
	extern GLuint cubeVao;
	extern GLuint cubeVbo[];
	extern GLuint cubeShaders[];
	extern GLuint cubeProgram;

	extern float cubeVerts[];
	extern GLubyte cubeIdx[];
}

namespace Cube {
	extern void setupCube();
	extern void cleanupCube();
	extern void updateCube(const glm::mat4& transform);
	extern void drawCube();
}

#pragma region SIMULATION
void InitPlanes();
glm::vec3 getPerpVector(glm::vec3 _a, glm::vec3 _b);
glm::vec4 getRectFormula(glm::vec3 _a, glm::vec3 _b, glm::vec3 _c, glm::vec3 _d);
bool checkWithPlane(const glm::vec3 originalPos, const glm::vec3 endPos, const glm::vec4 plano);

	std::vector<glm::vec4> planes;
	glm::vec3 acceleration = { 0,-9.81,0 };

	void InitPlanes() {
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
	bool checkWithPlane(const glm::vec3 originalPos, const glm::vec3 endPos, const glm::vec4 plano) {
		// (n * pt + d)(n*pt+dt + d)
		float x = (originalPos.x * plano.x) + (originalPos.y * plano.y) + (originalPos.z * plano.z) + plano.w;
		float y = (endPos.x * plano.x) + (endPos.y * plano.y) + (endPos.z * plano.z) + plano.w;
		return x * y < 0;
	}

#pragma endregion



class Rigidbody {
	glm::vec3 position;
	glm::vec3 size;

	void Init() {
		Cube::setupCube();
	}
	void Update() {
		Cube::updateCube(glm::mat4());
	}
	void Cleanup() {
		Cube::cleanupCube();
	}
};

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
	InitPlanes();
	Exemple_PhysicsInit();
}

void PhysicsUpdate(float dt) {
	Exemple_PhysicsUpdate(dt);
}

void PhysicsCleanup() {
	planes.clear();
}



