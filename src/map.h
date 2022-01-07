#ifndef MAP_H
#define MAP_H

#include "camera.h"

struct Map;

struct Map *map_new(void);
void map_destroy(struct Map *map);
float map_getheight(struct Map *map, float x, float z);
void map_render(struct Map *map, const struct Camera *cam);

#endif
