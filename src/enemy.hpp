#ifndef ENEMY_H
#define ENEMY_H

#include <GL/glew.h>
#include "camera.hpp"
#include "linalg.hpp"
#include "map.hpp"

class Enemy {
public:
	Enemy();
	~Enemy();
	Enemy(const Enemy&) = delete;

	float x, z;  // y depends on map
	void render(const Camera& camera, Map& map) const;
	void move_towards_player(vec3 player_location);

private:
	GLuint shaderprogram;
	GLuint vbo;  // Vertex Buffer Object, represents triangles going to gpu
};

#endif
