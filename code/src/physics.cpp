#include <imgui\imgui.h>
#include <imgui\imgui_impl_sdl_gl3.h>
#include <imgui\imgui_impl_sdl_gl3.h>
#include <glm\glm.hpp>
#include <glm\gtc\type_ptr.hpp>
#include <GL\glew.h>
#include <glm\gtc\matrix_transform.hpp>
#include <glm/gtx/closest_point.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#if __has_include(<ppl.h>)
#  include <ppl.h>
#  define ppl 1
#else
#  define ppl 0
#endif

float elasticity = 0.75f; // Aviso, bajar esto de 0.6 puede causar partículas que se salten colisiones si la aceleracion es muy alta.
int numIterations = 10; // TODO Meter en la UI
enum class ExecutionMode{STANDARD, PARALLEL};
ExecutionMode exMode = ExecutionMode::PARALLEL;
#if ppl == 1
static const char* ExecutionModeString[]{ "Sequential", "Parallel" };
#else
static const char* ExecutionModeString[]{ "Sequential", "Parallel (c++17 not available)" };
#endif
enum class SolverMode { VERLET, EULER };
SolverMode solverMode = SolverMode::VERLET;
static const char* SolverModeString[]{ "Verlet", "Euler" };

// Forward declarations
glm::vec4 getRectFormula(glm::vec3 _a, glm::vec3 _b, glm::vec3 _c, glm::vec3 _d);
glm::vec3 GetCascadeRotation(glm::vec3 dir, float angle);
glm::vec3 getPerpVector(glm::vec3 _a, glm::vec3 _b);
glm::vec3 eulerSolver(const glm::vec3 origin, const glm::vec3 end, const float _dt);
glm::vec3 verletSolver(const glm::vec3 lastPos, const glm::vec3 newPos, const glm::vec3 force, const float mass_, const float dt_);
glm::vec3 verletSolverSpeed(const glm::vec3 lastPos, const glm::vec3 newPos, const float dt_);

bool checkWithPlane(const glm::vec3 originalPos, const glm::vec3 endPos, const glm::vec4 plano);
glm::vec3 fixPos(const glm::vec3 originalPos, const glm::vec3 endPos, const glm::vec4 plano);
glm::vec3 fixSpeed(glm::vec3 originalSpeed, glm::vec3 endSpeed, glm::vec4 plano);
namespace Sphere {
	extern void setupSphere(glm::vec3 pos, float radius);
	extern void cleanupSphere();
	extern void updateSphere(glm::vec3 pos, float radius);

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
namespace Box {
	extern GLuint cubeVao;
	extern GLuint cubeVbo[];
	extern GLuint cubeShaders[];
	extern GLuint cubeProgram;

	extern float cubeVerts[];
	extern GLubyte cubeIdx[];
}


struct Spheres {
	float radius;
	glm::vec3 position;
	Spheres() : radius(2.0f), position({ 0,0,0 }) {}
	Spheres(float rad, glm::vec3 pos) : radius(rad), position(pos){}
};

struct Spring {
	int const ID;
	float const distance;
	Spring(int id_ = 0, float distance_ = 1) : ID(id_), distance(distance_) {};
};

bool CheckCollisionWithSphere(Spheres sphere, glm::vec3 primaPos);
glm::vec4 getPlaneFromSphere(glm::vec3 originalPos, glm::vec3 endPos, Spheres sphere);

struct Cloth {
	Spheres sphere = { 2.5f, {0, 2.5f, -0} };
	std::vector<glm::vec4> planes;
	glm::vec3 acceleration = { 0, -9.81f, 0 };
	glm::vec3  PARTICLE_START_POSITION = { 0,6,0 };
	float  PARTICLE_DISTANCE = .3f;
	bool Replay = true;
	int const RESOLUTION_X = 14;
	int const RESOLUTION_Y = 18;
	int const RESOLUTION = RESOLUTION_X * RESOLUTION_Y;

	glm::vec3* positions;
	glm::vec3* lastPositions;
	glm::vec3* primaPositions;
	glm::vec3* speeds;
	glm::vec3* primaSpeeds;
	glm::vec3* forces;

	float damping = 50.0f; // TODO Meter esto en la UI -> Valores 0 - 100
	float structuralK = 1000.0f; // Esto en laUI -> Valores 750 - 1250
	std::vector<Spring>* structuralSpringsBeta;
	float shearK = 1000.f;
	std::vector<Spring>* shearSpringsBeta;
	float bendK = 1000.f;
	std::vector<Spring>* bendSpringsBeta;

	float overDistance = 1.25f;
	std::vector<bool> staticParticles;

	float aliveTime = 0;
	const float MAX_TIME = 20;

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
		//std::cout << planes.size();
	}

	void InitParticles() {
		//std::cout << "Setting particles" << std::endl;
		//extern bool renderSphere;
		//renderSphere = true;
		sphere = { 2.5f, {0, 2.5f, 0} };
		sphere.position.x += (static_cast <float> (rand()) * 2 / static_cast <float> (RAND_MAX)) - 1;
		sphere.position.y += (static_cast <float> (rand()) * 2 / static_cast <float> (RAND_MAX)) - 1;
		sphere.position.z += (static_cast <float> (rand()) * 2 / static_cast <float> (RAND_MAX)) - 1;
		sphere.radius += ((static_cast <float> (rand()) * 2 / static_cast <float> (RAND_MAX)) - 1) * 2;
		float ydistance = (sphere.radius + sphere.position.y) - PARTICLE_START_POSITION.y + 0.1f;
		if (ydistance > 0) {
			sphere.position.y -= ydistance;
			ydistance = (sphere.radius + sphere.position.y) - PARTICLE_START_POSITION.y;
		}
		Sphere::setupSphere(sphere.position, sphere.radius);
		ClothMesh::setupClothMesh();
		if (planes.size() == 0) {
			InitPlanes();
		}
		positions = new glm::vec3[RESOLUTION];
		lastPositions = new glm::vec3[RESOLUTION];
		speeds = new glm::vec3[RESOLUTION];
		primaPositions = new glm::vec3[RESOLUTION];
		primaSpeeds = new glm::vec3[RESOLUTION];
		forces = new glm::vec3[RESOLUTION];
		structuralSpringsBeta = new std::vector<Spring>[RESOLUTION];
		shearSpringsBeta = new std::vector<Spring>[RESOLUTION];
		bendSpringsBeta = new std::vector<Spring>[RESOLUTION];
		staticParticles = std::vector<bool>(RESOLUTION, false); // Las partículas que no se actualizan
		LilSpheres::particleCount = 0;
		extern bool renderCloth;
		renderCloth = true;

		for (int i = 0; i < RESOLUTION; i++)
		{
			SpawnParticle(i);
		}
		staticParticles[0] = true;
		staticParticles[RESOLUTION_X - 1] = true;
		LilSpheres::particleCount = RESOLUTION;
	}
	void SpawnParticle(int i) {
		glm::vec3 position = PARTICLE_START_POSITION; // La inicializamos en el centro
		position.x -= (RESOLUTION_X / 2 * PARTICLE_DISTANCE) - PARTICLE_DISTANCE * .5f; // La colocamos en un punto de origen 
		position.z -= (RESOLUTION_Y / 2 * PARTICLE_DISTANCE) - PARTICLE_DISTANCE * .5f;
		position.x += i % RESOLUTION_X * PARTICLE_DISTANCE; // Setteamos su posicion original en base al offset con el tamaño del array
		position.z += ((i / RESOLUTION_X) % RESOLUTION_Y) * PARTICLE_DISTANCE; // Hacemos lo mismo pero con el offset en el otro eje

		///Structural

		/* Al dividir los numeros entre res_x y pasarlos a int, nos da la fila en la que está
		|0 ,1 ,2 ,3 ,4 | / 5 = 0
		|5 ,6 ,7 ,8 ,9 | / 5 = 1
		|10,11,12,13,14| / 5 = 2
		|15,16,17,18,19| / 5 = 3
		|20,21,22,23,24| / 5 = 4
		*/

		if ((int)(i / RESOLUTION_X) == (int)((i + 1) / RESOLUTION_X) && i + 1 < RESOLUTION) {
			structuralSpringsBeta[i].push_back(Spring(i + 1, PARTICLE_DISTANCE));
		}
		if ((int)(i / RESOLUTION_X) == (int)((i - 1) / RESOLUTION_X) && i - 1 >= 0) {
			structuralSpringsBeta[i].push_back(Spring(i - 1, PARTICLE_DISTANCE));
		}
		if (i + RESOLUTION_X < RESOLUTION) {
			structuralSpringsBeta[i].push_back(Spring(i + RESOLUTION_X, PARTICLE_DISTANCE));
		}
		if (i - RESOLUTION_X >= 0) {
			structuralSpringsBeta[i].push_back(Spring(i - RESOLUTION_X, PARTICLE_DISTANCE));
		}

		///Shear
		if ((int)((i + 1 + RESOLUTION_X) / RESOLUTION_X) - (int)(i / RESOLUTION_X) == 1 && i + 1 + RESOLUTION_X < RESOLUTION) {
			shearSpringsBeta[i].push_back(Spring(i + 1 + RESOLUTION_X, sqrtf(PARTICLE_DISTANCE * PARTICLE_DISTANCE + PARTICLE_DISTANCE * PARTICLE_DISTANCE)));
		}

		if ((int)((i - 1 + RESOLUTION_X) / RESOLUTION_X) - (int)(i / RESOLUTION_X) == 1 && i - 1 + RESOLUTION_X < RESOLUTION) {
			shearSpringsBeta[i].push_back(Spring(i - 1 + RESOLUTION_X, sqrtf(PARTICLE_DISTANCE * PARTICLE_DISTANCE + PARTICLE_DISTANCE * PARTICLE_DISTANCE)));
		}

		if ((int)(i / RESOLUTION_X) - (int)((i + 1 - RESOLUTION_X) / RESOLUTION_X) == 1 && i + 1 - RESOLUTION_X >= 0) {
			shearSpringsBeta[i].push_back(Spring(i + 1 - RESOLUTION_X, sqrtf(PARTICLE_DISTANCE * PARTICLE_DISTANCE + PARTICLE_DISTANCE * PARTICLE_DISTANCE)));
		}

		if ((int)(i / RESOLUTION_X) - (int)((i - 1 - RESOLUTION_X) / RESOLUTION_X) == 1 && i - 1 - RESOLUTION_X >= 0) {
			shearSpringsBeta[i].push_back(Spring(i - 1 - RESOLUTION_X, sqrtf(PARTICLE_DISTANCE * PARTICLE_DISTANCE + PARTICLE_DISTANCE * PARTICLE_DISTANCE)));
		}


		///Bend
		if ((int)(i / RESOLUTION_X) - (int)((i - 2 * RESOLUTION_X) / RESOLUTION_X) == 2 && i - 2 * RESOLUTION_X >= 0) {
			bendSpringsBeta[i].push_back(Spring(i - 2 * RESOLUTION_X, PARTICLE_DISTANCE * 2));
		}
		if ((int)(i / RESOLUTION_X) - (int)((i + 2 * RESOLUTION_X) / RESOLUTION_X) == -2 && i + 2 * RESOLUTION_X < RESOLUTION) {
			bendSpringsBeta[i].push_back(Spring(i + 2 * RESOLUTION_X, PARTICLE_DISTANCE * 2));
		}

		if ((int)(i / RESOLUTION_X) == (int)((i - 2) / RESOLUTION_X) && i - 2 >= 0) {
			bendSpringsBeta[i].push_back(Spring(i - 2, PARTICLE_DISTANCE * 2));
		}
		if ((int)(i / RESOLUTION_X) == (int)((i + 2) / RESOLUTION_X) && i + 2 < RESOLUTION) {
			bendSpringsBeta[i].push_back(Spring(i + 2, PARTICLE_DISTANCE * 2));
		}


		forces[i] = { 0, -9.81f, 0 };
		lastPositions[i] = position;
		positions[i] = position;
	}
	void UpdateParticles(float dt) {

		if (Replay) {
			aliveTime += dt / numIterations;
			if (aliveTime > MAX_TIME) {
				ResetParticles();
			}
		}

		switch (exMode)
		{
		case ExecutionMode::PARALLEL:
#if ppl == 1
			Concurrency::parallel_for(0, (int)RESOLUTION, [this, dt](int i) {
				UpdateClothParticle(i);
				});
			Concurrency::parallel_for(0, (int)RESOLUTION, [this, dt](int i) {
				UpdateParticle(i, dt);
				});
#endif
			break;
		default:
			for (int i = 0; i < RESOLUTION; i++)
			{
				UpdateClothParticle(i);
			}

			for (int i = 0; i < RESOLUTION; i++)
			{
				UpdateParticle(i, dt);
			}
			break;
		}
		LilSpheres::updateParticles(0, RESOLUTION, &positions[0].x);
		ClothMesh::updateClothMesh(&positions[0].x);
	}

	void UpdateClothParticle(int i) {
		glm::vec3 tempvector = forces[i] = glm::vec3(0,0,0);
		for (std::vector<Spring>::iterator it = structuralSpringsBeta[i].begin(); it != structuralSpringsBeta[i].end(); it++)
		{
			tempvector += fixForces(i, it->ID, structuralK, damping, it->distance);
		}

		// Fix forces for Shear
		for (std::vector<Spring>::iterator it = shearSpringsBeta[i].begin(); it != shearSpringsBeta[i].end(); it++)
		{
			forces[i] += fixForces(i, it->ID, shearK, damping, it->distance);
		}
		
		// Fix forces for Bending
		for (std::vector<Spring>::iterator it = bendSpringsBeta[i].begin(); it != bendSpringsBeta[i].end(); it++)
		{
			forces[i] += fixForces(i, it->ID, bendK, damping, it->distance);
		}
		forces[i] = tempvector + acceleration;
		
	}

	void UpdateCollisions(int i) {
		extern bool renderSphere;
		if (renderSphere)
			if (CheckCollisionWithSphere(sphere, primaPositions[i])) {
				glm::vec4 plano = getPlaneFromSphere(positions[i], primaPositions[i], sphere);
				primaPositions[i] = fixPos(positions[i], primaPositions[i], plano);
				if (solverMode == SolverMode::EULER)
					primaSpeeds[i] = fixSpeed(speeds[i], primaSpeeds[i], plano);
			}
		for (int it = 0; it < planes.size(); it++) {
			// Para evitar problemas, comprobar siempre los planos lo último
			if (checkWithPlane(positions[i], primaPositions[i], planes[it])) {
				primaPositions[i] = fixPos(positions[i], primaPositions[i], planes[it]);
				if (solverMode == SolverMode::EULER)
					primaSpeeds[i] = fixSpeed(speeds[i], primaSpeeds[i], planes[it]);
			}
		}
	}

	void UpdateParticle(int i, float dt) {
		if (!staticParticles[i])
		 {
			switch (solverMode)
			{
			case SolverMode::VERLET:
			{
				primaPositions[i] = verletSolver(lastPositions[i], positions[i], forces[i], 1.f, dt);
				lastPositions[i] = positions[i];
				speeds[i] = verletSolverSpeed(positions[i], primaPositions[i], dt);
				UpdateCollisions(i);
				positions[i] = primaPositions[i];

				
			}
				break;
			case SolverMode::EULER:
			{
				primaPositions[i] = eulerSolver(positions[i], speeds[i], dt);
				primaSpeeds[i] = eulerSolver(speeds[i], acceleration, dt);

				UpdateCollisions(i);

				speeds[i] = primaSpeeds[i];
				positions[i] = primaPositions[i];
			}
				break;
			default:
				break;
			}
		}
	}
	void CleanParticles() {
		Sphere::cleanupSphere();
		ClothMesh::cleanupClothMesh();
		delete[] positions;
		delete[] lastPositions;
		delete[] speeds;
		delete[] primaPositions;
		delete[] primaSpeeds;
		delete[] forces;

		delete[] structuralSpringsBeta;
		delete[] shearSpringsBeta;
		delete[] bendSpringsBeta;

		staticParticles.clear();
	}
	void ResetParticles() {
		aliveTime = 0;
		CleanParticles();
		InitParticles();
	}
	glm::vec3 fixForces(int i, int j, float ke, float kd, float distance) {
		glm::vec3 resultantForce = glm::vec3(0, 0, 0);
		if (i == j)
			return resultantForce;

		// F = -(ke(||P1-P2|| - Length) + kd(v1 - v2) * {(P1-P2)/(||P1-P2||)}) *{(P1-P2)/(||P1-P2||)}
		glm::vec3 dirVect = positions[i] - positions[j]; // Nota, se puede multiplicar vect por float pero no vect por int .-.
		float mag = glm::length<float>(dirVect);
		glm::vec3 tmpVec = dirVect / mag;
		float partA = ke * (mag - distance);
		float partB = glm::dot<float>(kd * (speeds[i] - speeds[i - 1]), tmpVec);
		resultantForce = -(partA + partB) * tmpVec;

		

		return resultantForce;
	}

} parts;


bool show_test_window = false;


void GUI() {
	bool show = true;
	ImGui::Begin("Physics Parameters", &show, 0);

	{
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);//FrameRate
	}

	ImGui::NewLine();
	ImGui::Text("System options:");
	ImGui::Combo("Computing mode", (int*)(&exMode), ExecutionModeString, 2);
#if ppl == 0
	if (exMode == ExecutionMode::PARALLEL) exMode = ExecutionMode::STANDARD;
#endif
	SolverMode lastSolver = solverMode;
	ImGui::Combo("Solver mode", (int*)(&solverMode), SolverModeString, 2);
	if (lastSolver != solverMode) parts.ResetParticles();
	if(solverMode == SolverMode::EULER)
		ImGui::SliderFloat("Elasticity", &elasticity, .5f, 1);
	ImGui::DragFloat3("Global Acceleration", &parts.acceleration[0], .01f);

	ImGui::NewLine();
	ImGui::Text("Cloth options:");
	extern bool renderCloth;
	ImGui::Checkbox("Draw cloth", &renderCloth);
	extern bool renderParticles;
	ImGui::Checkbox("Draw particles", &renderParticles);
	ImGui::Checkbox("Auto reset", &parts.Replay);
	if (parts.Replay) {
		ImGui::Text("Reset in %.1f seconds", parts.MAX_TIME - parts.aliveTime);
	}
	ImGui::Spacing();
	ImGui::DragFloat3("Start position ", &parts.PARTICLE_START_POSITION[0], .01f);
	ImGui::DragFloat("Cloth distance ", &parts.PARTICLE_DISTANCE, .01f);
	ImGui::Spacing();
	//ImGui::DragFloat("Damping ", &parts.damping, .01f);
	ImGui::SliderFloat("Structural K ", &parts.structuralK, 0, .5f);
	ImGui::SliderFloat("Shear K ", &parts.shearK, 0, .5f);
	ImGui::SliderFloat("Bend K ", &parts.bendK, 0, .5f);

	ImGui::Spacing();
	if (ImGui::Button("Reset")) {
		parts.ResetParticles();
	}


	ImGui::NewLine();
	ImGui::Text("Sphere:");

	extern bool renderSphere;
	ImGui::Checkbox("Enable", &renderSphere);
	if (renderSphere) {
		ImGui::Spacing();
		ImGui::SliderFloat("Radius", &parts.sphere.radius, 0, 10);
		ImGui::DragFloat3("Position ", &parts.sphere.position[0], .01f);
		Sphere::updateSphere(parts.sphere.position, parts.sphere.radius);
	}
	ImGui::End();
}

void PhysicsInit() {
	srand(time(NULL));
	extern bool renderSphere;
	renderSphere = true;
	parts.InitParticles();
}

void PhysicsUpdate(float dt) {
	for (int i = 0; i < numIterations; i++) {
		parts.UpdateParticles(dt);
	}
}

void PhysicsCleanup() {
	parts.CleanParticles();
}

glm::vec3 GetCascadeRotation(glm::vec3 k, float angle) {

	// Obtenemos un vector perpendicular cualquiera, basándonos en el que ya tenemos, esta es la forma mas simple
	glm::vec3 v = { k.y, -k.x, 0 };

	// Aplicamos la "Rodrigues Rotation Formula" que nos permite rotar un vector cualquiera en un eje personalizado
	// v' = v cosθ + (k x v)sinθ + k(k*v)(1-cosθ)  <--- Esta es la formula
	// Donde K = el vector director de la cascada, y V es el vector que queremos rotar
	glm::vec3 newRotation = v * cos(glm::radians(angle)) + (glm::cross(k, v) * sin(glm::radians(angle))) + k * (glm::dot(k, v)) * (1 - cos(glm::radians(angle)));
	return glm::normalize(newRotation); // Lo normalizamos para usarlo en base a la strength
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
bool CheckCollisionWithSphere(Spheres sphere, glm::vec3 primaPos) {
	return glm::distance(sphere.position, primaPos) <= sphere.radius;
}

glm::vec4 getPlaneFromSphere(glm::vec3 originalPos, glm::vec3 endPos, Spheres sphere) {

	// Vector director
		// f(x) = originalPos + lambda * Director;  
	glm::vec3 director = endPos - originalPos; 
	/*  // NO BORRAR --> Explicacion de como funciona el calculo de la esfera
	|| Pcol		-	C		|| ^2 = r^2 --> PCol = OP + lV
	||OP + lV	-	C		|| ^2 = r^2
	||	lV		+	(OP-C)	|| ^2 ...
	||	lV		+	CP		|| ^2 = r^2
	dot(lv+CP, lv+CP) = r^2 
	dot(lv, lv+CP)+dot(CP,(lV+CP)) = r2;
	l^2 = dot(v,v) 
	l = 2*dot(v, CP)
	- = dot(CP,CP) - r2
	*/
	glm::vec3 CP = originalPos -sphere.position;
	float square = glm::dot(director,director);
	float normal = glm::dot(director, CP) + glm::dot(CP, director);
	float number = glm::dot(CP,CP) - (sphere.radius*sphere.radius);

	double sqrt = glm::sqrt((normal*normal) - (4 * square*number));


	double lambda = glm::min((-normal + sqrt) / 2 * square, (-normal - sqrt) / 2 * square);
	// Punto de impacto
	glm::vec3 impact = originalPos + glm::vec3{ lambda * director.x, lambda * director.y, lambda * director.z}; // Si, me daba pereza sobreescribir la multiplicacion por la del double
	// Plano tangente
	glm::vec3 normalPlano = glm::normalize(impact-sphere.position);
	float D = (normalPlano.x * impact.x * -1) + (normalPlano.y * impact.y * -1) + (normalPlano.z * impact.z * -1);
	return { normalPlano.x, normalPlano.y, normalPlano.z, D};
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

glm::vec3 eulerSolver(const glm::vec3 origin, const glm::vec3 end, const float _dt) {
	return origin + _dt * end;
}

glm::vec3 verletSolver(const glm::vec3 lastPos, const glm::vec3 newPos, const glm::vec3 force, const float mass_, const float dt_) {
	return newPos + (newPos - lastPos) + (force / mass_) * glm::pow((dt_ / numIterations), 2);
}
glm::vec3 verletSolverSpeed(const glm::vec3 currentPos, const glm::vec3 primaPos, const float dt_) {
	return (primaPos - currentPos) / (dt_ / numIterations);
}

bool checkWithPlane(const glm::vec3 originalPos, const glm::vec3 endPos, const glm::vec4 plano) {
	// (n * pt + d)(n*pt+dt + d)
	float x = (originalPos.x * plano.x) + (originalPos.y * plano.y) + (originalPos.z * plano.z) + plano.w;
	float y = (endPos.x * plano.x) + (endPos.y * plano.y) + (endPos.z * plano.z) + plano.w;
	return x * y < 0;
}
glm::vec3 fixPos(const glm::vec3 originalPos, const glm::vec3 endPos, const glm::vec4 plano) {
	glm::vec3 newPos = { 0,0,0 };
	glm::vec3 normalPlano = { plano.x, plano.y, plano.z };
	newPos = (endPos - (2 * (glm::dot(endPos, normalPlano) + plano.w)) * normalPlano); // Podr�amos usar glm::reflect
	return newPos;

}
glm::vec3 fixSpeed(glm::vec3 originalSpeed, glm::vec3 endSpeed, glm::vec4 plano) {
	glm::vec3 newSpeed = { 0,0,0 };
	glm::vec3 normalPlano = { plano.x, plano.y, plano.z };
	newSpeed = (endSpeed - (2 * (glm::dot(endSpeed, normalPlano))) * normalPlano) * elasticity;

	return newSpeed;
}



