#include <stdio.h>
#include "map.h"
#include "camera.h"
#include "log.h"

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

	while(1) {
		SDL_Event e;
		SDL_WaitEvent(&e);
		if (e.type == SDL_QUIT)
			break;

		struct Camera cam = {
			.surface = SDL_GetWindowSurface(wnd),
			.screencentery = CAMERA_SCREEN_HEIGHT/2,
			.world2cam.rows = {{1,0,0},{0,1,0},{0,0,1}},
			.cam2world.rows = {{1,0,0},{0,1,0},{0,0,1}},
		};
		camera_update_visplanes(&cam);
		map_drawgrid(&sect, &cam);
		log_printf("drew");
		SDL_UpdateWindowSurface(wnd);
	}

	SDL_DestroyWindow(wnd);
	return 0;
}
