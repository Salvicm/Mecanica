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

namespace LilSpheres {
	extern void updateParticles(int startIdx, int count, float* array_data);
	extern int particleCount;

}

#pragma region SIMULATION
void InitPlanes();
glm::vec3 getPerpVector(glm::vec3 _a, glm::vec3 _b);
glm::vec4 getRectFormula(glm::vec3 _a, glm::vec3 _b, glm::vec3 _c, glm::vec3 _d);
bool checkWithPlane(const glm::vec3 originalPos, const glm::vec3 endPos, const glm::vec4 plano);

std::vector<glm::vec4> planes;
float resetTime = 15;
float countdown = 0;
float speed = 1;
glm::vec3 acceleration = { 0.0f,-.981f,0.0f };
bool simulate = true;
int frameCount = 0;

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
public:
	//BASIC
	glm::vec3 position = { 0.0f,5.0f,0.0f };
	glm::vec3 size = { 1.0f, 1.0f, 1.0f };
	glm::fquat orientation;
	glm::vec3 angularMomentum;

	//ASSIST
	glm::vec3 linearAcceleration;
	glm::vec3 force = { 0,0,0 };
	glm::vec3 forcePosition = { 0,0,0 };

	glm::fquat angularAcceleration;


	//CONSTANTS
	float mass = 1;
	float inverseMass;
	glm::vec3 angularInertia = { 0,0,0 };


	void Init() {
		Cube::setupCube();
		extern bool renderCube;
		renderCube = true;


		position = { 0.0f,5.0f,0.0f };
		size = { 1.0f, 1.0f, 1.0f };
		orientation = { 0,0,0,1 };
		angularMomentum = { 0,0,0 };

		linearAcceleration = { 0,0,0 };

		angularAcceleration = { 0,0,0,1 };


		angularInertia.x = mass * (glm::exp2(size.y) + glm::exp2(size.z)) / 12;
		angularInertia.y = mass * (glm::exp2(size.x) + glm::exp2(size.z)) / 12;
		angularInertia.z = mass * (glm::exp2(size.x) + glm::exp2(size.y)) / 12;
		inverseMass = 1 / mass;
		force.x = ((static_cast <float> (rand()) * 2 / static_cast <float> (RAND_MAX)) - 1) * 10;
		force.y = ((static_cast <float> (rand()) * 2 / static_cast <float> (RAND_MAX)) - 1) * 10;
		force.z = ((static_cast <float> (rand()) * 2 / static_cast <float> (RAND_MAX)) - 1) * 10;
		forcePosition = position;
		forcePosition.x += (static_cast <float> (rand()) * 2 / static_cast <float> (RAND_MAX)) - 1;
		forcePosition.y += (static_cast <float> (rand()) * 2 / static_cast <float> (RAND_MAX)) - 1;
		forcePosition.z += (static_cast <float> (rand()) * 2 / static_cast <float> (RAND_MAX)) - 1;

		extern bool renderParticles;
		renderParticles = true;

		LilSpheres::particleCount = 20;
		glm::vec3 pointsPos[20];
		glm::vec3 tempPos = forcePosition;
		for (size_t i = 0; i < 20; i++)
		{
			pointsPos[i] = tempPos;
			tempPos += force * float(0.005f * i);
		}
		LilSpheres::updateParticles(0, 20, &pointsPos[0].x);

	}
	void Update(float _dt) {
		SemiImplicitEuler(_dt);
		glm::mat4 t = glm::translate(glm::mat4(), glm::vec3(position.x, position.y, position.z)); // Esto es temporal
		glm::mat4 r = glm::toMat4(orientation);
		glm::mat4 s = glm::scale(glm::mat4(), glm::vec3(size.x, size.y, size.z));
		Cube::updateCube(t * r * s);
	}
	void Cleanup() {
		Cube::cleanupCube();
		extern bool renderCube;
		renderCube = false;
	}

	void Reset() {
		countdown = 0;
		frameCount = 0;
		Cleanup();
		Init();
	}


	void SemiImplicitEuler(float _dt) {
		linearAcceleration += (force + acceleration) * inverseMass;
		position += _dt * linearAcceleration;

		angularMomentum += glm::cross(force, (forcePosition - position));

		angularAcceleration = angularMomentum * angularInertia;
		orientation += _dt * (angularAcceleration * orientation);
	}

} rigidBod;

bool show_test_window = false;
void GUI() {
	bool show = true;
	ImGui::Begin("Practica rigidbodies", &show, 0);

	{
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::Text("Simulation Settings:");
		ImGui::Text("Next reset %.3f : %.3f", countdown, resetTime);
		ImGui::DragFloat3("Acceleration", &acceleration[0], .01f);
		ImGui::SliderFloat("Reset time", &resetTime, .5f, 20.f);
		ImGui::SliderFloat("Simulation speed", &speed, 0, 1.5f);
		ImGui::Checkbox("Simulate", &simulate);


		ImGui::Spacing();
		ImGui::Text("Object Settings:");
		ImGui::DragFloat3("Cube Position", &rigidBod.position[0], .01f);
		ImGui::DragFloat3("Cube Size", &rigidBod.size[0], .01f);


		if (ImGui::Button("Reset")) {
			rigidBod.Reset();
		}
	}

	ImGui::End();
}

void PhysicsInit() {
	srand(time(NULL));
	InitPlanes();
	rigidBod.Init();
}

void PhysicsUpdate(float dt) {
	if (countdown >= resetTime) {
		rigidBod.Reset();
	}
	if (simulate) {
		if (frameCount > 0) {
			rigidBod.force = { 0,0,0 };
		}
		rigidBod.Update(dt * speed);
	}
	countdown += dt;
	frameCount++;
}

void PhysicsCleanup() {
	planes.clear();
	rigidBod.Cleanup();
}