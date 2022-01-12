#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <cstdlib>
#include <ctime>
#include "camera.hpp"
#include "enemy.hpp"
#include "linalg.hpp"
#include "log.hpp"
#include "map.hpp"
#include "opengl_boilerplate.hpp"

static double counter_in_seconds()
{
	return SDL_GetPerformanceCounter() / static_cast<double>(SDL_GetPerformanceFrequency());
}

int main(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	std::srand(std::time(nullptr));

	OpenglBoilerplate boilerplate = {};
	Map map = {};
	Enemy enemy = {vec3{0,0,-50}};
	Camera camera = {};
	PhysicsObject player = {vec3{0,50,0}};

	int zdir = 0;
	int angledir = 0;
	float angle = 0;

	double last_time = counter_in_seconds();

	while (1) {
		double remaining_delta_time;
		for (double delta_time = counter_in_seconds() - last_time; delta_time > 0; delta_time = remaining_delta_time) {
			float dt = 0.02f;
			remaining_delta_time = delta_time - dt;
			if (remaining_delta_time < 0)
				dt = static_cast<float>(delta_time);

			angle += 1.8f*dt*angledir;
			camera.cam2world = mat3::rotation_about_y(angle);
			camera.world2cam = mat3::rotation_about_y(-angle);

			player.update(map, dt);
			camera.location = player.get_location() + vec3{0,5,0};

			enemy.move_towards_player(camera.location, map, dt);
		}
		last_time = counter_in_seconds();

		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		map.render(camera);
		enemy.render(camera, map);
		SDL_GL_SwapWindow(boilerplate.window);

		SDL_Event e;
		while (SDL_PollEvent(&e)) switch(e.type) {
			case SDL_QUIT:
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

		player.set_extra_force(camera.cam2world * vec3{0, 0, 40.0f*zdir});
	}
}
