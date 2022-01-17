#include <SDL2/SDL.h>
#include "config.hpp"
#include "physics.hpp"
#include "camera.hpp"
#include "player.hpp"

Player::Player(float initial_height) : physics_object(vec3(0,initial_height,0)) {}

void Player::render(const Camera& camera, Map& map) const
{
	// TODO
}

void Player::move_and_turn(int z_direction, int angle_direction, Map& map, float dt)
{
	SDL_assert(z_direction == 0 || z_direction == -1 || z_direction == 1);
	SDL_assert(angle_direction == 0 || angle_direction == -1 || angle_direction == 1);
	this->physics_object.set_extra_force(this->camera.cam2world * vec3{0, 0, PLAYER_MOVING_FORCE*z_direction});
	this->physics_object.update(map, dt);
}
