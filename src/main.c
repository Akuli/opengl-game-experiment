#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include "map.h"
#include "camera.h"
#include "linalg.h"
#include "log.h"

#define MOVING_SPEED 2.0f
#define TURNING_SPEED 2.0f

int main(void)
{
	srand(time(NULL));

	SDL_Window *wnd = SDL_CreateWindow("title", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, CAMERA_SCREEN_WIDTH, CAMERA_SCREEN_HEIGHT, 0);
	SDL_assert(wnd);

	uint64_t delay = SDL_GetPerformanceFrequency() / CAMERA_FPS;

	struct Map map = {0};
	struct Camera cam = {
		.screencentery = CAMERA_SCREEN_HEIGHT/2,
		.world2cam.rows = {{1,0,0},{0,1,0},{0,0,1}},
		.cam2world.rows = {{1,0,0},{0,1,0},{0,0,1}},
		.location = {1, map_getheight(&map, 1, 1) + 1, 1},
	};

	int zdir = 0, angledir = 0;
	float camangle = 0;

	double percentsum = 0;
	int percentcount = 0;

	while(1) {
		uint64_t start = SDL_GetPerformanceCounter();

		SDL_Event e;
		while (SDL_PollEvent(&e)) switch(e.type) {
		case SDL_QUIT:
			SDL_DestroyWindow(wnd);
			map_freebuffers(&map);
			return 0;

		case SDL_KEYDOWN:
			switch(e.key.keysym.scancode) {
			case SDL_SCANCODE_W:
				zdir = -1;
				break;
			case SDL_SCANCODE_S:
				zdir = 1;
				break;
			case SDL_SCANCODE_A:
				angledir = -1;
				break;
			case SDL_SCANCODE_D:
				angledir = 1;
				break;
			default:
				break;
			}
			break;

		case SDL_KEYUP:
			switch(e.key.keysym.scancode) {
			case SDL_SCANCODE_W:
				if (zdir == -1)
					zdir = 0;
				break;
			case SDL_SCANCODE_S:
				if (zdir == 1)
					zdir = 0;
				break;
			case SDL_SCANCODE_A:
				if (angledir == -1)
					angledir = 0;
				break;
			case SDL_SCANCODE_D:
				if (angledir == 1)
					angledir = 0;
				break;
			default:
				break;
			}
			break;

		default:
			break;
		}

		if (zdir != 0) {
			vec3_add_inplace(&cam.location, mat3_mul_vec3(cam.cam2world, (Vec3){ 0, 0, zdir*MOVING_SPEED/CAMERA_FPS }));
			cam.location.y = map_getheight(&map, cam.location.x, cam.location.z) + 1;
		}

		if (angledir != 0) {
			camangle += angledir*TURNING_SPEED/CAMERA_FPS;
			cam.cam2world = mat3_rotation_xz(camangle);
			cam.world2cam = mat3_rotation_xz(-camangle);
		}

		// surface can change as events are handled, or be NULL when game starts
		cam.surface = SDL_GetWindowSurface(wnd);
		if (cam.surface) {
			camera_update_visplanes(&cam);
			SDL_FillRect(cam.surface, NULL, 0);
			map_drawgrid(&map, &cam);
			SDL_UpdateWindowSurface(wnd);
		}

		double percent = (SDL_GetPerformanceCounter() - start) / (double)delay;
		if (percent > 1)
			log_printf("event loop is lagging: %.2f%%", percent*100);

		percentsum += percent;
		percentcount++;
		if (percentcount == CAMERA_FPS) {
			log_printf("perf average: %.2f%%", percentsum/percentcount*100);
			percentsum = 0;
			percentcount = 0;
		}

		uint64_t remains;
		// If no time remaining, subtraction below overflows (unsigned overflow isn't ub)
		while ((remains = start + delay - SDL_GetPerformanceCounter()) < (uint64_t)(0.9*(double)UINT64_MAX)) {
			if (remains > SDL_GetPerformanceFrequency()/1000)
				SDL_Delay(1);
		}
	}
}
