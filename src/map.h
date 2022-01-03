#ifndef MAP_H
#define MAP_H

#include "camera.h"

struct GaussianCurveMountain {
	/*
	y = yscale*e^(-(((x - centerx) / xzscale)^2 + ((z - centerz) / xzscale)^2))
	yscale can be negative, xzscale can't, center must be within map
	*/
	float xzscale, yscale, centerx, centerz;
};

struct Section {
	int startx, startz;
	struct GaussianCurveMountain mountains[100];
};

struct Map {
	struct Section *sections;
	int nsections;
};

float map_getheight(struct Map *map, float x, float z);
void map_drawgrid(struct Map *map, const struct Camera *cam);


#endif
