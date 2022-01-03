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

#define SECTION_SIZE 8  // side length of section square on xz plane
#define YTABLE_ITEMS_PER_UNIT 5  // how frequently to cache computed heights

struct Section {
	int startx, startz;
	struct GaussianCurveMountain mountains[100];

	// Cached values for height of map, depends on heights of this and neighbor sections
	float ytable[SECTION_SIZE*YTABLE_ITEMS_PER_UNIT + 1][SECTION_SIZE*YTABLE_ITEMS_PER_UNIT + 1];
	bool ytableready;
};

struct Map {
	struct Section *sections;
	int nsections;

	int *itable;  // Hash table of indexes into sections array
	unsigned sectsalloced;  // space allocated in itable and sections
};

float map_getheight(struct Map *map, float x, float z);
void map_drawgrid(struct Map *map, const struct Camera *cam);


#endif
