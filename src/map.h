#ifndef MAP_H
#define MAP_H

#include "camera.h"

#define MAP_SECTION_SIZE 16

struct GaussianCurveMountain {
	/*
	y = yscale*e^(-xzscale*((x - centerx)^2 + (z - centerz)^2))
	yscale can be negative, xzscale can't, center must be within map
	*/
	float xzscale, yscale, centerx, centerz;
};

struct Section {
	int startx, startz;
	struct GaussianCurveMountain mountains[16];
};

void map_drawgrid(const struct Section *sect, const struct Camera *cam);


#endif
