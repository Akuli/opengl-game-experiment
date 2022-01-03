#include "map.h"
#include "camera.h"
#include "log.h"
#include <math.h>

#define GRID_LINES_PER_UNIT 10

static float uniform_random_float(float min, float max)
{
	return min + rand()*(max-min)/(float)RAND_MAX;
}

void map_generate(struct Section *sect)
{
	int i;
	int n = sizeof(sect->mountains)/sizeof(sect->mountains[0]);

	// wide and smooth
	for (i = 0; i < n/20; i++) {
		float h = 1.0f - expf(uniform_random_float(-10, 2));
		float w = uniform_random_float(fabsf(h), 3*fabsf(h));
		sect->mountains[i] = (struct GaussianCurveMountain){
			.xzscale = w,
			.yscale = h,
			.centerx = uniform_random_float(sect->startx, sect->startx + MAP_SECTION_SIZE),
			.centerz = uniform_random_float(sect->startz, sect->startz + MAP_SECTION_SIZE),
		};
	}

	// narrow and pointy
	for (; i < n; i++) {
		float h = uniform_random_float(0.05f, 0.3f);
		if (rand() % 2)
			h = -h;
		float w = uniform_random_float(2*fabsf(h), 5*fabsf(h));
		sect->mountains[i] = (struct GaussianCurveMountain){
			.xzscale = w,
			.yscale = h,
			.centerx = uniform_random_float(sect->startx, sect->startx + MAP_SECTION_SIZE),
			.centerz = uniform_random_float(sect->startz, sect->startz + MAP_SECTION_SIZE),
		};
	}
}

float map_getheight(const struct Section *sect, float x, float z)
{
	float y = 0;
	for (int i = 0; i < sizeof(sect->mountains)/sizeof(sect->mountains[0]); i++) {
		float dx = x - sect->mountains[i].centerx;
		float dz = z - sect->mountains[i].centerz;
		float xzscale = sect->mountains[i].xzscale;
		y += sect->mountains[i].yscale * expf(-1/(xzscale*xzscale) * (dx*dx + dz*dz));
	}
	return y;
}

void map_drawgrid(const struct Section *sect, const struct Camera *cam)
{
	static float heights[MAP_SECTION_SIZE*GRID_LINES_PER_UNIT + 1][MAP_SECTION_SIZE*GRID_LINES_PER_UNIT + 1];

	for (int xidx = 0; xidx <= MAP_SECTION_SIZE*GRID_LINES_PER_UNIT; xidx++) {
		for (int zidx = 0; zidx <= MAP_SECTION_SIZE*GRID_LINES_PER_UNIT; zidx++) {
			float x = sect->startx + xidx * (1.0f/GRID_LINES_PER_UNIT);
			float z = sect->startz + zidx * (1.0f/GRID_LINES_PER_UNIT);
			float y = map_getheight(sect, x, z);
			heights[xidx][zidx] = y;
		}
	}

	for (int xidx = 0; xidx < MAP_SECTION_SIZE*GRID_LINES_PER_UNIT; xidx++) {
		for (int zidx = 0; zidx < MAP_SECTION_SIZE*GRID_LINES_PER_UNIT; zidx++) {
			float x1 = sect->startx + xidx * (1.0f/GRID_LINES_PER_UNIT);
			float x2 = sect->startx + (xidx+1) * (1.0f/GRID_LINES_PER_UNIT);
			float z1 = sect->startz + zidx * (1.0f/GRID_LINES_PER_UNIT);
			float z2 = sect->startz + (zidx+1) * (1.0f/GRID_LINES_PER_UNIT);

			float y_x1z1 = heights[xidx][zidx];
			float y_x2z1 = heights[xidx+1][zidx];
			float y_x1z2 = heights[xidx][zidx+1];

			camera_drawline(cam, (Vec3){x1, y_x1z1, z1}, (Vec3){x1, y_x1z2, z2});
			camera_drawline(cam, (Vec3){x1, y_x1z1, z1}, (Vec3){x2, y_x2z1, z1});
		}
	}
}
