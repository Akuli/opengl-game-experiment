#include "map.h"
#include "camera.h"
#include "log.h"
#include <math.h>

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

#define GRID_LINES_PER_UNIT 10
#define RADIUS 5

void map_drawgrid(const struct Section *sect, const struct Camera *cam)
{
	float spacing = 1.0f / GRID_LINES_PER_UNIT;

	static float heights[2*RADIUS*GRID_LINES_PER_UNIT + 1][2*RADIUS*GRID_LINES_PER_UNIT + 1];

	// Middle element of heights goes to this location
	float midx = roundf(cam->location.x / spacing) * spacing;
	float midz = roundf(cam->location.z / spacing) * spacing;

	for (int xidx = 0; xidx <= 2*RADIUS*GRID_LINES_PER_UNIT; xidx++) {
		for (int zidx = 0; zidx <= 2*RADIUS*GRID_LINES_PER_UNIT; zidx++) {
			float x = midx + (xidx - RADIUS*GRID_LINES_PER_UNIT)*spacing;
			float z = midz + (zidx - RADIUS*GRID_LINES_PER_UNIT)*spacing;
			float dx = x - cam->location.x;
			float dz = z - cam->location.z;
			if (dx*dx + dz*dz > RADIUS*RADIUS)
				heights[xidx][zidx] = NAN;
			else
				heights[xidx][zidx] = map_getheight(sect, x, z);
		}
	}

	for (int xidx = 0; xidx < 2*RADIUS*GRID_LINES_PER_UNIT; xidx++) {
		for (int zidx = 0; zidx < 2*RADIUS*GRID_LINES_PER_UNIT; zidx++) {
			float y = heights[xidx][zidx];
			if (isnan(y))
				continue;
			float x = midx + (xidx - RADIUS*GRID_LINES_PER_UNIT)*spacing;
			float z = midz + (zidx - RADIUS*GRID_LINES_PER_UNIT)*spacing;

			if (!isnan(heights[xidx+1][zidx]))
				camera_drawline(cam, (Vec3){x,y,z}, (Vec3){x+spacing,heights[xidx+1][zidx],z});
			if (!isnan(heights[xidx][zidx+1]))
				camera_drawline(cam, (Vec3){x,y,z}, (Vec3){x,heights[xidx][zidx+1],z+spacing});
		}
	}
}
