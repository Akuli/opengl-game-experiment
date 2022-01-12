#ifndef ENEMY_H
#define ENEMY_H

#include <GL/glew.h>
#include "camera.hpp"
#include "linalg.hpp"
#include "map.hpp"
#include "physics.hpp"

class Enemy {
public:
	Enemy(vec3 initial_location);
	~Enemy();
	Enemy(const Enemy&) = delete;
	Enemy(Enemy&&) = default;

	void render(const Camera& camera, Map& map) const;
	void move_towards_player(vec3 player_location, Map& map, float dt);

	static void decide_location(vec3 player_location, float& x, float& z);

	PhysicsObject physics_object;  // TODO private
private:
	GLuint shaderprogram;
	GLuint vbo;  // Vertex Buffer Object, represents triangles going to gpu
};

#endif
