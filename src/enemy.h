#ifndef ENEMY_H
#define ENEMY_H

#include "camera.h"

struct Enemy;

struct Enemy *enemy_new(void);
void enemy_destroy(struct Enemy *enemy);
void enemy_render(struct Enemy *enemy, const struct Camera *cam);

#endif
