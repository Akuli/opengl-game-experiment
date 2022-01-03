#include <stdio.h>
#include <stdint.h>
#include "map.h"
#include "camera.h"
#include "log.h"

#define MOVING_SPEED 1.0f

int main(void)
{
	struct Section sect = {
		.mountains = {
			// xzscale, yscale, centerx, centerz
			{ 1, 1, 1, -3 },
		},
		.startx = 0,
		.startz = -10,
	};

	SDL_Window *wnd = SDL_CreateWindow("title", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, CAMERA_SCREEN_WIDTH, CAMERA_SCREEN_HEIGHT, 0);
	SDL_assert(wnd);

	uint64_t delay = SDL_GetPerformanceFrequency() / CAMERA_FPS;

	struct Camera cam = {
		.screencentery = CAMERA_SCREEN_HEIGHT/2,
		.world2cam.rows = {{1,0,0},{0,1,0},{0,0,1}},
		.cam2world.rows = {{1,0,0},{0,1,0},{0,0,1}},
	};

	int xdir = 0, zdir = 0;

	while(1) {
		uint64_t end = SDL_GetPerformanceCounter() + delay;

		SDL_Event e;
		while (SDL_PollEvent(&e)) switch(e.type) {
		case SDL_QUIT:
			SDL_DestroyWindow(wnd);
			return 0;

		case SDL_KEYDOWN:
			switch(e.key.keysym.scancode) {
			case SDL_SCANCODE_W:
				zdir = -1;
				break;
			case SDL_SCANCODE_S:
				zdir = 1;
				break;
			default:
				break;
			}
			break;

		case SDL_KEYUP:
			switch(e.key.keysym.scancode) {
			case SDL_SCANCODE_W:
			case SDL_SCANCODE_S:
				zdir = 0;
				break;
			default:
				break;
			}
			break;

		default:
			break;
		}

		cam.location.x += xdir*MOVING_SPEED/CAMERA_FPS;
		cam.location.z += zdir*MOVING_SPEED/CAMERA_FPS;

		// surface can change as events are handled, or be NULL when game starts
		cam.surface = SDL_GetWindowSurface(wnd);
		if (cam.surface) {
			camera_update_visplanes(&cam);
			SDL_FillRect(cam.surface, NULL, 0);
			map_drawgrid(&sect, &cam);
			SDL_UpdateWindowSurface(wnd);
		}

		uint64_t remains;
		// If no time remaining, subtraction below overflows (unsigned overflow isn't ub)
		while ((remains = end - SDL_GetPerformanceCounter()) < (uint64_t)(0.9*(double)UINT64_MAX)) {
			if (remains > SDL_GetPerformanceFrequency()/1000)
				SDL_Delay(1);
		}
	}
}
