#ifndef ENEMY_H
#define ENEMY_H

#include <GL/glew.h>
#include "camera.hpp"
#include "map.hpp"

struct Enemy {
	GLuint shaderprogram;
	GLuint vbo;  // Vertex Buffer Object, represents triangles going to gpu
};

Enemy enemy_new(void);
void enemy_destroy(const Enemy *enemy);
void enemy_render(const Enemy *en, const Camera *cam, Map& map);

#endif
