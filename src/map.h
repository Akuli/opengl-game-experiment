#ifndef MAP_H
#define MAP_H

#include "camera.h"

struct GaussianCurveMountain {
	/*
	y = yscale*e^(-(((x - centerx) / xzscale)^2 + ((z - centerz) / xzscale)^2))
	yscale can be negative, xzscale can't, center must be within map
	center coords are relative to section start, so that sections are easy to move
	*/
	float xzscale, yscale, centerx, centerz;
};

#define SECTION_SIZE 8  // side length of section square on xz plane
#define YTABLE_ITEMS_PER_UNIT 5  // how frequently to cache computed heights

struct Section {
	int startx, startz;
	struct GaussianCurveMountain mountains[100];

	/*
	Cached values for height of map, depends on heights of this and neighbor sections
	Raw version does not take in account neighbours and is always ready.
	This way, sections can be generated beforehand and added to the map quickly as needed.
	*/
	float ytableraw[3*SECTION_SIZE*YTABLE_ITEMS_PER_UNIT + 1][3*SECTION_SIZE*YTABLE_ITEMS_PER_UNIT + 1];
	float ytable[SECTION_SIZE*YTABLE_ITEMS_PER_UNIT + 1][SECTION_SIZE*YTABLE_ITEMS_PER_UNIT + 1];
	bool ytableready;
};

struct Map {
	struct Section *sections;
	int nsections;

	int *itable;  // Hash table of indexes into sections array
	unsigned sectsalloced;  // space allocated in itable and sections

	// queue of sections to add to map later, when they are needed
	struct Section queue[15];
	int queuelen;
};

float map_getheight(struct Map *map, float x, float z);
void map_drawgrid(struct Map *map, const struct Camera *cam);

/*
Run this when idle, if needed generates one section and adds this to queue.
You typically need many new sections at once, because neighbor sections affect the section
that needs to be added. If there's nothing in queue, that's slow.
*/
void map_prepare_section(struct Map *map);

void map_freebuffers(const struct Map *map);


#endif
