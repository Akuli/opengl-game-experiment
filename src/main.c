#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include "map.h"
#include "camera.h"
#include "linalg.h"
#include "log.h"
#include "opengl_boilerplate.h"

int main(void)
{
	srand(time(NULL));

	struct Map *map = map_new();
	struct Camera cam = {
		.world2cam.rows = {{1,0,0},{0,1,0},{0,0,1}},
		.cam2world.rows = {{1,0,0},{0,1,0},{0,0,1}},
	};

	struct OpenglBoilerplateState bpstate = opengl_boilerplate_init();

	int zdir = 0;

	while (1) {
		cam.location.z += 0.3f*zdir;  // FIXME: should depend on fps
		cam.location.y = map_getheight(map, cam.location.x, cam.location.z) + 5;

		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glUniform3f(bpstate.camloc_uniform, cam.location.x, cam.location.y, cam.location.z);
		map_drawgrid(map, &cam);
		SDL_GL_SwapWindow(bpstate.window);

		SDL_Event e;
		while (SDL_PollEvent(&e)) switch(e.type) {
			case SDL_QUIT:
				opengl_boilerplate_quit(&bpstate);
				return 0;

			case SDL_KEYDOWN:
				switch(e.key.keysym.scancode) {
					case SDL_SCANCODE_W: zdir = -1; break;
					case SDL_SCANCODE_S: zdir = 1; break;
					default: break;
				}
				break;

			case SDL_KEYUP:
				switch(e.key.keysym.scancode) {
					case SDL_SCANCODE_W: if (zdir == -1) zdir = 0; break;
					case SDL_SCANCODE_S: if (zdir == 1) zdir = 0; break;
					default: break;
				}
				break;

			default:
				break;
		}
	}
}
