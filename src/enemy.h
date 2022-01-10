#ifndef ENEMY_H
#define ENEMY_H

#include <GL/glew.h>
#include "camera.h"
#include "map.h"

struct Enemy {
	GLuint shaderprogram;
	GLuint vbo;  // Vertex Buffer Object, represents triangles going to gpu
};

struct Enemy enemy_new(void);
void enemy_destroy(const struct Enemy *enemy);
void enemy_render(const struct Enemy *enemy, const struct Camera *cam, struct Map *map);

#endif
