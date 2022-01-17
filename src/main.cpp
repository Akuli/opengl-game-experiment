#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <vector>
#include "config.hpp"
#include "enemy.hpp"
#include "linalg.hpp"
#include "log.hpp"
#include "map.hpp"
#include "opengl_boilerplate.hpp"
#include "player.hpp"

static double counter_in_seconds()
{
	return SDL_GetPerformanceCounter() / static_cast<double>(SDL_GetPerformanceFrequency());
}

struct GameState {
	Map map;
	Player player = Player(map.get_height(0,0));
	float camera_angle;
	double start_time = counter_in_seconds();
	double next_enemy_time = counter_in_seconds();

	GameState(const GameState &) = delete;

	void add_enemy_if_needed() {
		if (counter_in_seconds() < this->next_enemy_time)
			return;

		float x, z;
		Enemy::decide_location(this->player.get_location(), x, z);
		this->map.add_enemy(Enemy(vec3{ x, this->map.get_height(x, z), z }));

		/*
		Later in the game, produce enemies more quickly.
		DO NOT use something like "enemy_delay *= 0.99" because that will result in
		an exponentially small enemy delay, i.e. too many enemies.
		*/
		double minutes_passed = (this->next_enemy_time - this->start_time)/60;
		double enemy_delay = 1/(1 + minutes_passed);
		this->next_enemy_time += enemy_delay;

		log_printf("Added an enemy, now there are %d enemies and next adding will happen after %.2fsec",
			(int)this->map.get_number_of_enemies(), enemy_delay);
	}

	void update_physics(int z_direction, int angle_direction, float dt) {
		this->player.move_and_turn(z_direction, angle_direction, this->map, dt);
		this->map.move_enemies(this->player.get_location(), dt);
	}
};

int main(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	std::srand(std::time(nullptr));

	OpenglBoilerplate boilerplate = {};
	GameState game_state = {};

	int zdir = 0;
	int angledir = 0;

	double last_time = counter_in_seconds();

	while (1) {
		for (double rem = counter_in_seconds() - last_time; rem > 0; rem -= MIN_PHYSICS_STEP_SECONDS)
		{
			float dt = static_cast<float>(std::min(rem, MIN_PHYSICS_STEP_SECONDS));
			game_state.update_physics(zdir, angledir, dt);
		}
		last_time = counter_in_seconds();

		game_state.add_enemy_if_needed();

		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		game_state.map.render(game_state.player.camera);
		game_state.player.render(game_state.player.camera, game_state.map);
		for (const Enemy* e : game_state.map.find_enemies_within_circle(game_state.player.get_location().x, game_state.player.get_location().z, VIEW_RADIUS))
		{
			e->render(game_state.player.camera, game_state.map);
		}
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
	}
}
