#ifndef MAP_H
#define MAP_H

#include "camera.h"

struct Map;

struct Map *map_new(void);
void map_destroy(struct Map *map);
float map_getheight(struct Map *map, float x, float z);
void map_render(struct Map *map, const struct Camera *cam);

// If the floor has constant height, returns identity matrix.
// In general, returns a rotation that maps (0,1,0) to be perpendicular to the map.
mat3 map_get_rotation(struct Map *map, float x, float z);

#endif
