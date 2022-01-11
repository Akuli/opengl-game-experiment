#ifndef ENEMY_H
#define ENEMY_H

#include <GL/glew.h>
#include "camera.hpp"
#include "map.hpp"

class Enemy {
public:
	Enemy();
	~Enemy();
	Enemy(const Enemy&) = delete;

	void render(const Camera& camera, Map& map) const;

private:
	GLuint shaderprogram;
	GLuint vbo;  // Vertex Buffer Object, represents triangles going to gpu
};

#endif
