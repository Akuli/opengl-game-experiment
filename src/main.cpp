#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <memory>
#include "camera.hpp"
#include "config.hpp"
#include "enemy.hpp"
#include "linalg.hpp"
#include "log.hpp"
#include "map.hpp"
#include "opengl_boilerplate.hpp"
#include "physics.hpp"

static double counter_in_seconds()
{
	return SDL_GetPerformanceCounter() / static_cast<double>(SDL_GetPerformanceFrequency());
}

struct GameState {
	Map map;
	PhysicsObject player = PhysicsObject(vec3{0, map.get_height(0,0), 0});
	Camera camera;
	float camera_angle;
	std::vector<std::unique_ptr<Enemy>> enemies;

	GameState(const GameState &) = delete;

	double next_enemy_time = counter_in_seconds() + ENEMY_DELAY;
	void add_enemy_if_needed() {
		if (counter_in_seconds() < this->next_enemy_time)
			return;

		log_printf("There are %d enemies, adding one more", (int)this->enemies.size());
		float x, z;
		Enemy::decide_location(this->player.get_location(), x, z);
		this->enemies.push_back(std::make_unique<Enemy>(vec3{ x, this->map.get_height(x, z), z }));
		this->next_enemy_time += ENEMY_DELAY;
	}

	void update_physics(float dt, int camera_angle_direction) {
		this->camera_angle += PLAYER_TURNING_SPEED*dt*camera_angle_direction;
		this->camera.cam2world = mat3::rotation_about_y(this->camera_angle);
		this->camera.world2cam = mat3::rotation_about_y(-this->camera_angle);

		this->player.update(this->map, dt);
		for (const std::unique_ptr<Enemy>& e : this->enemies) {
			e->move_towards_player(this->camera.location, this->map, dt);
		}
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

	double last_time = counter_in_seconds() - 1e-5;  // Make sure at least one physics iteration runs below

	while (1) {
		for (double rem = counter_in_seconds() - last_time; rem > 0; rem -= MIN_PHYSICS_STEP_SECONDS)
		{
			float dt = static_cast<float>(std::min(rem, MIN_PHYSICS_STEP_SECONDS));
			game_state.update_physics(dt, angledir);
		}
		last_time = counter_in_seconds();

		game_state.camera.location = game_state.player.get_location() + vec3{0,CAMERA_HEIGHT,0};

		game_state.add_enemy_if_needed();

		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		game_state.map.render(game_state.camera);
		for (const std::unique_ptr<Enemy>& e : game_state.enemies)
			e->render(game_state.camera, game_state.map);
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

		game_state.player.set_extra_force(game_state.camera.cam2world * vec3{0, 0, PLAYER_MOVING_FORCE*zdir});
	}
}
