﻿#include <imgui\imgui.h>
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

// Debug
void coutVec3(glm::vec3 tmp) {
	std::cout << tmp.x << ", " << tmp.y << ", " << tmp.z << std::endl;
}
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

int randomSeed = 10;

std::vector<glm::vec4> planes;
float resetTime = 15;
float countdown = 0;
float speed = 1;
glm::vec3 acceleration = { 0.0f, -.981f,0.0f };
bool simulate = true;
int frameCount = 0;

void InitPlanes() {
	planes.push_back(getRectFormula(
		// Basandonos en los indices del cubo 
		{ -Box::cubeVerts[4 * 3], -Box::cubeVerts[4 * 3 + 1], -Box::cubeVerts[4 * 3 + 2] },
		{ -Box::cubeVerts[5 * 3], -Box::cubeVerts[5 * 3 + 1], -Box::cubeVerts[5 * 3 + 2] },
		{ -Box::cubeVerts[6 * 3], -Box::cubeVerts[6 * 3 + 1], -Box::cubeVerts[6 * 3 + 2] },
		{ -Box::cubeVerts[7 * 3], -Box::cubeVerts[7 * 3 + 1], -Box::cubeVerts[7 * 3 + 2] }));
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
glm::vec3 LinePoint(const Line & line, float range) {
	return line.point + range * line.direction;
}
glm::vec3 LinePlaneCollision(const Line & line, const glm::vec4 & plane) {
	return LinePoint(line, LinePlaneCollisionRange(line, plane));
}
#pragma endregion


const int obj_points = 8;

class Rigidbody {
public:
	//BASIC
	glm::vec3 position = { 0.0f,5.0f,0.0f };
	glm::vec3 cubePoints[obj_points];
	glm::vec3 lastPosition;
	glm::vec3 size = { 1.0f, 1.0f, 1.0f };
	glm::fquat orientation;
	glm::fquat lastOrientation;
	glm::vec3 angularMomentum;
	glm::vec3 lastAngularMomentum;
	glm::vec3 linearMomentum = { 0.0f,0.0f,0.0f };
	glm::vec3 lastLinearMomentum;
	glm::vec3 inertiaTensor = { 0.0f,0.0f,0.0f };
	glm::vec3 lastInertiaTensor = { 0.0f,0.0f,0.0f };
	// DEBUG
	bool showIntersections = false;
	//ASSIST
	glm::vec3 force = { 0,0,0 };
	float forceScale = 10;
	glm::vec3 forcePosition = { 0,0,0 };

	float tolerance = 0.1f;
	float elasticity = 0.25f;
	//CONSTANTS
	float mass = 1;
	float inverseMass = 0;
	glm::vec3 iBody = { 0,0,0 };
	struct CollisionInfo {
		bool collided = false;
		int point = -1;
		float distance = 0;
	};

	void Init() {
		srand(randomSeed);
		Cube::setupCube();
		extern bool renderCube;
		renderCube = true;

		position.x = ((static_cast <float> (rand()) * 2 / static_cast <float> (RAND_MAX)) - 1) * 4;
		position.y = ((static_cast <float> (rand()) / static_cast <float> (RAND_MAX))) * 8 + 1;
		position.z = ((static_cast <float> (rand()) * 2 / static_cast <float> (RAND_MAX)) - 1) * 4;
		orientation = { 0,0,0,1 };
		orientation.x = ((static_cast <float> (rand()) * 2 / static_cast <float> (RAND_MAX)) - 1);
		orientation.y = ((static_cast <float> (rand()) * 2 / static_cast <float> (RAND_MAX)) - 1);
		orientation.z = ((static_cast <float> (rand()) * 2 / static_cast <float> (RAND_MAX)) - 1);
		angularMomentum = { 0,0,0 };
		linearMomentum = { 0, 0, 0 };

		iBody.x = mass * (glm::exp2(size.y) + glm::exp2(size.z)) / 12;
		iBody.y = mass * (glm::exp2(size.x) + glm::exp2(size.z)) / 12;
		iBody.z = mass * (glm::exp2(size.x) + glm::exp2(size.y)) / 12;
		inverseMass = 1 / mass;

		cubePoints[0] = { .5f, .5f, .5f };
		cubePoints[1] = { -.5f, .5f, .5f };
		cubePoints[2] = { -.5f, -.5f, .5f };
		cubePoints[3] = { -.5f, -.5f, -.5f };
		cubePoints[4] = { .5f, -.5f, .5f };
		cubePoints[5] = { .5f, -.5f, -.5f };
		cubePoints[6] = { .5f, .5f, -.5f };
		cubePoints[7] = { -.5f, .5f, -.5f };

		force.x = ((static_cast <float> (rand()) * 2 / static_cast <float> (RAND_MAX)) - 1)* forceScale;
		force.y = ((static_cast <float> (rand()) * 2 / static_cast <float> (RAND_MAX)) - 1)* forceScale;
		force.z = ((static_cast <float> (rand()) * 2 / static_cast <float> (RAND_MAX)) - 1)* forceScale;

		forcePosition.x = (static_cast <float> (rand()) / static_cast <float> (RAND_MAX)) - 0.5;
		forcePosition.y = (static_cast <float> (rand()) / static_cast <float> (RAND_MAX)) - 0.5;
		forcePosition.z = (static_cast <float> (rand()) / static_cast <float> (RAND_MAX)) - 0.5;
		forcePosition = getRelativePoint(forcePosition);

		extern bool renderParticles;
		renderParticles = true;

		LilSpheres::particleCount = 20 + 6 * obj_points;
		glm::vec3 pointsPos[20];
		glm::vec3 tempPos = forcePosition;
		for (size_t i = 0; i < 20; i++)
		{
			pointsPos[i] = tempPos;
			tempPos += force * float(0.005f * i);
		}
		LilSpheres::updateParticles(0, 20, &pointsPos[0].x);
	}
	void Update(const float _dt) {
		
		lastPosition = position;
		lastOrientation = orientation;
		lastAngularMomentum = angularMomentum;
		lastLinearMomentum = linearMomentum;
		lastInertiaTensor = inertiaTensor;

		SemiImplicitEuler(_dt);

		for (size_t i = 0; i < planes.size(); i++)
		{
			CollisionInfo coll = intersectionWithPlane(i);
			if (coll.collided) {
				float scalar = 0.5f;
				glm::vec3 normal = planes[i];
				normal *= -1;
				int k = 0;
				while (abs(coll.distance) > tolerance) {
					SemiImplicitEuler(_dt * scalar);
					coll.distance = LinePlaneCollisionRange({ getRelativePoint(cubePoints[coll.point]), normal }, planes[i]);
					float scale = 1 - scalar;
					if (coll.distance > 0) {
						scalar += scale * 0.5;
					}
					else {
						scalar -= scale * 0.5;
					}
					if (k > 10000) {
						// en el caso de que haya demasiadas comprovaciones puede que haya fallado algo, preferimos reiniciar a que crashee
						std::cout << "Demasiadas comprovaciones de colision" << std::endl;
						Reset();
						break;
					}
					k++;
				}

				// Colision y posicion corregida
				
				forcePosition = getRelativePoint(cubePoints[coll.point]);
		
				glm::vec3 pato = lastLinearMomentum + glm::cross(lastAngularMomentum, (cubePoints[coll.point]));
				float relVel = glm::dot(normal, pato); // pb(t0) = 0
				float parteDeArriba = -(1 + elasticity/2.0f) * relVel;
				// normal del plano dot product
				glm::vec3 crossHelp = glm::cross(cubePoints[coll.point], normal);
				crossHelp = lastInertiaTensor * crossHelp; 
				glm::vec3 normCross = glm::cross(crossHelp, cubePoints[coll.point]);
				float tmp = glm::dot(normal, normCross);
				// Tensor de inercia * cross(Punto relativo, normal del plano)
				// cross con Ra
				// + 0
				float j = parteDeArriba / (inverseMass + tmp);
				force = j * normal;
	
		

		
				glm::vec3 pointsPos[20];
				glm::vec3 tempPos = forcePosition;
				for (size_t i = 0; i < 20; i++) 
				{
					pointsPos[i] = tempPos;
					tempPos += force * float(0.005f * i);
				}
				LilSpheres::updateParticles(0, 20, &pointsPos[0].x);

				linearMomentum = { 0,0,0 };
				angularMomentum = { 0,0,0 };
				lastPosition = position;
				lastOrientation = orientation;
				lastAngularMomentum = angularMomentum;
				lastLinearMomentum = linearMomentum;
				lastInertiaTensor = inertiaTensor;
				SemiImplicitEuler(_dt * (1 - scalar));
			}
		}
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
		simulate = true;
		countdown = 0;
		frameCount = 0;
		Cleanup();
		Init();
	}
	CollisionInfo intersectionWithPlane(int i) {
		glm::vec3 normal = planes[i];
		normal *= -1;
			glm::vec3 pointsPos[obj_points];
			if (showIntersections) {
				for (size_t j = 0; j < obj_points; j++)
				{
					pointsPos[j] = LinePlaneCollision({ getRelativePoint(cubePoints[j]), normal }, planes[i]);
				}
			}
			else {
				for (size_t j = 0; j < obj_points; j++)
				{
					pointsPos[j] = position;
				}
			}
			LilSpheres::updateParticles(20 + i * obj_points, obj_points, &pointsPos[0].x);

		float scales[obj_points];
		for (size_t j = 0; j < obj_points; j++)
		{
			scales[j] = LinePlaneCollisionRange({ getRelativePoint(cubePoints[j]), normal }, planes[i]);
		}

		CollisionInfo collision;
		for (size_t i = 0; i < obj_points; i++)
		{
			if (scales[i] <= collision.distance) {
				collision.collided = true;
				collision.point = i;
				collision.distance = scales[i];
			}
		}

		return collision;
	}

	glm::vec3 getRelativePoint(glm::vec3 point) {
		point *= size;
		point = orientation * point;
		point += position;
		return point;
	}

	void SemiImplicitEuler(const float _dt) {
		position = lastPosition;
		orientation = lastOrientation;
		inertiaTensor = lastInertiaTensor;
		angularMomentum = lastAngularMomentum;
		linearMomentum = lastLinearMomentum;

		//POSITION
		/*
		P(t+dt) = P(t) + dt * F(t); // Linear momentum
		L(t+dt) = L(t) + dt * te(t); // Angular momentum
		v(t+dt) = P(t+dt) / M; // Velocity
		x(t+dt) = x(t) + dt * v(t + dt); // Position
		*/
		linearMomentum += (acceleration + force); // Linear momentum
		glm::vec3 linearSpeed = linearMomentum * inverseMass;
		position += _dt * linearSpeed;


		//ROTATION

		inertiaTensor = glm::toMat3(orientation) * iBody * glm::toMat3(orientation);
		angularMomentum += glm::cross(force, (forcePosition - position)) * -iBody; // Momento angular
		orientation = glm::normalize(orientation);
		glm::fquat q(0, angularMomentum.x, angularMomentum.y, angularMomentum.z);
		glm::fquat spin = 0.5f * q * orientation;
		orientation += _dt * spin;
		
	}

} rigidBod;

bool show_test_window = false;
void GUI() {
	bool show = true;
	ImGui::Begin("[AA4 - Alegre/Calvillo]Practica rigidbodies", &show, 0);
	{
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::Text("Simulation Settings:");
		ImGui::DragInt("Seed", &randomSeed);
		ImGui::Text("Next reset %.3f : %.3f", countdown, resetTime);
		ImGui::DragFloat3("Acceleration", &acceleration[0], .01f);
		ImGui::SliderFloat("Reset time", &resetTime, .5f, 20.f);
		ImGui::SliderFloat("Simulation speed", &speed, 0, 1.5f);
		ImGui::Checkbox("Simulate", &simulate);


		ImGui::Spacing();
		ImGui::Text("Object Settings:");
		ImGui::DragFloat3("Cube Position", &rigidBod.position[0], .01f);
		ImGui::DragFloat3("Cube Size", &rigidBod.size[0], .01f);
		ImGui::SliderFloat("Elasticity", &rigidBod.elasticity, .0f, 0.5f);
		ImGui::SliderFloat("Tolerance", &rigidBod.tolerance, .001f, .1f);
		ImGui::SliderFloat("Mass", &rigidBod.mass, .01f, 5.f);
		ImGui::SliderFloat("ForceScale", &rigidBod.forceScale, .01f, 20.f);


		ImGui::Spacing();
		ImGui::Text("Debug: ");
		ImGui::Checkbox("Show Intersections", &rigidBod.showIntersections);


		if (ImGui::Button("Reset")) {
			rigidBod.Reset();
		}
	}

	ImGui::End();
}

void PhysicsInit() {
	randomSeed = time(NULL);
	InitPlanes();
	rigidBod.Init();
}

void PhysicsUpdate(float dt) {
	if (simulate) {
		if (countdown >= resetTime) {
			randomSeed = time(NULL);
			rigidBod.Reset();
		}
		if (frameCount > 0) {
			rigidBod.force = { 0,0,0 };
		}
		rigidBod.Update(dt * speed);
		countdown += dt * speed;
		frameCount++;
	}
}

void PhysicsCleanup() {
	planes.clear();
	rigidBod.Cleanup();
}