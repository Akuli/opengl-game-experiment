#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctime>
#include "map.hpp"
#include "camera.hpp"
#include "linalg.hpp"
#include "opengl_boilerplate.hpp"
#include "enemy.hpp"

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

	std::srand(std::time(NULL));

	struct OpenglBoilerplateState bpstate = opengl_boilerplate_init();

	struct Map *map = map_new();
	struct Enemy en = enemy_new();
	struct Camera cam = {};

	int zdir = 0;
	int angledir = 0;
	float angle = 0;

	while (1) {
		// FIXME: turn amount should depend on fps
		angle += 0.03f*angledir;
		cam.cam2world = mat3::rotation_about_y(angle);
		cam.world2cam = mat3::rotation_about_y(-angle);

		// FIXME: move amount should depend on fps
		cam.location += cam.cam2world * vec3{0,0,0.3f*zdir};
		cam.location.y = map_getheight(map, cam.location.x, cam.location.z) + 5;

		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		map_render(map, &cam);
		enemy_render(&en, &cam, map);
		SDL_GL_SwapWindow(bpstate.window);

		SDL_Event e;
		while (SDL_PollEvent(&e)) switch(e.type) {
			case SDL_QUIT:
				map_destroy(map);
				enemy_destroy(&en);
				opengl_boilerplate_quit(&bpstate);
				return 0;

			case SDL_KEYDOWN:
				switch(e.key.keysym.scancode) {
					case SDL_SCANCODE_W: zdir = -1; break;
					case SDL_SCANCODE_S: zdir = 1; break;
					case SDL_SCANCODE_A: angledir = -1; break;
					case SDL_SCANCODE_D: angledir = 1; break;
					default: break;
				}
				break;

			case SDL_KEYUP:
				switch(e.key.keysym.scancode) {
					case SDL_SCANCODE_W: if (zdir == -1) zdir = 0; break;
					case SDL_SCANCODE_S: if (zdir == 1) zdir = 0; break;
					case SDL_SCANCODE_A: if (angledir == -1) angledir = 0; break;
					case SDL_SCANCODE_D: if (angledir == 1) angledir = 0; break;
					default: break;
				}
				break;

			default:
				break;
		}
	}
}
