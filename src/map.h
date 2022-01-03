#ifndef MAP_H
#define MAP_H

#include "camera.h"

#define MAP_SECTION_SIZE 8

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

void map_generate(struct Section *sect);
float map_getheight(const struct Section *sect, float x, float z);
void map_drawgrid(const struct Section *sect, const struct Camera *cam);


#endif
