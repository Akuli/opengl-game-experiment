#include "map.h"
#include "camera.h"
#include "log.h"
#include <math.h>

#define GRID_LINES_PER_UNIT 10

static float calculate_y(const struct Section *sect, float x, float z)
{
	float y = 0;
	for (int i = 0; i < sizeof(sect->mountains)/sizeof(sect->mountains[0]); i++) {
		float dx = x - sect->mountains[i].centerx;
		float dz = z - sect->mountains[i].centerz;
		y += sect->mountains[i].yscale * expf(-sect->mountains[i].xzscale * (dx*dx + dz*dz));
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
			float y = calculate_y(sect, x, z);
			heights[xidx][zidx] = y;

			for (int i = 0; i < sizeof(cam->visplanes)/sizeof(cam->visplanes[0]); i++) {
				if (!plane_whichside(cam->visplanes[i], (Vec3){x,y,z})) {
					heights[xidx][zidx] = NAN;
					goto next_point;
				}
			}

		next_point:
			continue;
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

			if (!isnan(y_x1z1) && !isnan(y_x1z2))
				camera_drawline(cam, (Vec3){x1, y_x1z1, z1}, (Vec3){x1, y_x1z2, z2});
			if (!isnan(y_x1z1) && !isnan(y_x2z1))
				camera_drawline(cam, (Vec3){x1, y_x1z1, z1}, (Vec3){x2, y_x2z1, z1});
		}
	}
}
