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
		.location = {1, map_getheight(map, 1, 1) + 1, 1},
	};

	struct OpenglBoilerplateState bpstate = opengl_boilerplate_init();

	while (1) {
		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		map_drawgrid(map, &cam);
		// TODO: update vertex data and display it
		/*
		vertex_data[2] -= 0.01f;

		glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_object);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertex_data), vertex_data);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glDrawArrays(GL_TRIANGLES, 0, sizeof(vertex_data)/(3*sizeof(float)));

		glDisableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		*/

		SDL_GL_SwapWindow(bpstate.window);

		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT) {
				opengl_boilerplate_quit(&bpstate);
				return 0;
			}
		}
	}
}
