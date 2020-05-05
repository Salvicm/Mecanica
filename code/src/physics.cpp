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
typedef glm::tquat<float> quaternion;
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
float resetTime = 15;
glm::vec3 acceleration = { 0,-9.81,0 };
bool simulate = true;

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

void SemiImplicitEuler() {
	/*
	P(t+dt) = P(t) + dt * F(t); // Position
	L(t+dt) = L(t) + dt * te(t); // Angular momentum
	v(t+dt) = P(t+dt) / M; // Velocity
	x(t+dt) = x(t) + dt * v(t + dt); // Center of mass position
	I(t)^-1 = R(t) * i(body)^-1 * R(t)^T; // Inertia tensor
	w(t) = I(t)^-1 * L(T+dt); // Angular speed
	R(t+dt) = R(t) + dt* (w(t) * R(t)); // Rotation
	
	*/
}
#pragma endregion



class Rigidbody {
public:
	glm::vec3 position = { 0.0f,0.0f,0.0f }; // Center mass
	glm::vec3 deltaPosition = { 0.0f,0.0f,0.0f }; // Center mass
	glm::vec3 size = {1.0f, 1.0f, 1.0f};
	glm::vec3 acceleration;
	glm::vec3 speed;
	glm::vec3 angularSpeed;
	glm::vec3 torque;
	glm::mat3 inertiaTensor;

	quaternion rotationQuatern;
	glm::vec3 rotationVect;
	float tolerancy = 0.5f;
	float elasticityCoeff = 0.5f;
	
	void Init() {
		Cube::setupCube();
		extern bool renderCube;
		renderCube = true;
	}
	void Update() {
		glm::mat4 t = glm::translate(glm::mat4(), glm::vec3(position.x, position.y, position.z)); // Esto es temporal
		glm::mat4 r = glm::mat4(1.f);
		glm::mat4 s = glm::scale(glm::mat4(), glm::vec3(size.x, size.y, size.z));
		Cube::updateCube(t * r * s);
		Cube::drawCube();
	}
	void Cleanup() {
		Cube::cleanupCube();
		extern bool renderCube;
		renderCube = false;
	}
} rigidBod;

bool show_test_window = false;
void GUI() {
	bool show = true;
	ImGui::Begin("Practica rigidbodies", &show, 0);

	{	
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);//FrameRate
		ImGui::Text("");
		ImGui::Text("Solo debug");
		ImGui::DragFloat3("Cube Position", &rigidBod.position[0], .01f);
		ImGui::DragFloat3("Cube Size", &rigidBod.size[0], .01f);

		ImGui::DragFloat3("Acceleration", &acceleration[0], .01f);
		ImGui::SliderFloat("Reset time", &resetTime, .5f, 1.5f);
		ImGui::Checkbox("Simulate", &simulate);
		ImGui::SliderFloat("Elasticity coefficient", &rigidBod.elasticityCoeff, .5f, 1.5f);
		ImGui::SliderFloat("Tolerancy", &rigidBod.tolerancy, .5f, 1.5f);
		}
	
	ImGui::End();
}

void PhysicsInit() {
	InitPlanes();
	rigidBod.Init();
}

void PhysicsUpdate(float dt) {
	rigidBod.Update();
}

void PhysicsCleanup() {
	planes.clear();
	rigidBod.Cleanup();
}



