#include "player.hpp"
#include <SDL2/SDL.h>
#include <cmath>
#include <functional>
#include "config.hpp"
#include "physics.hpp"
#include "camera.hpp"
#include "misc.hpp"
#include "surface.hpp"
#include "linalg.hpp"

static vec4 tu_to_3d_point_and_brightness(vec2 tu)
{
	using std::cos, std::sin, std::log;
	float t = tu.x, u = tu.y;
	float r = 2 + cos(u);
	return vec4{ r*cos(t), (1 + sin(u)), r*sin(t), lerp<float>(0.3f, 0.6f, unlerp(-1,1,-cos(t))) };
}

static Surface surface = Surface(
	tu_to_3d_point_and_brightness,
	0, 2*std::acos(-1.0f), 50,
	0, 2*std::acos(-1.0f), 50,
	1.0f, 0.6f, 0.0f);

Player::Player(float initial_height) : physics_object(&surface, vec3(0,initial_height,0)) {}


static void smooth_clamp_below(float& value, float min)
{
	value -= min;
	value = 0.5f*(value + std::sqrt(value*value + 10));
	value += min;
}

void Player::move_and_turn(int z_direction, int angle_direction, Map& map, float dt)
{
	SDL_assert(z_direction == 0 || z_direction == -1 || z_direction == 1);
	SDL_assert(angle_direction == 0 || angle_direction == -1 || angle_direction == 1);

	this->camera_angle += PLAYER_TURNING_SPEED*dt*angle_direction;
	this->camera.cam2world = mat3::rotation_about_y(this->camera_angle);
	this->camera.world2cam = mat3::rotation_about_y(-this->camera_angle);

	this->physics_object.set_extra_force(this->camera.cam2world * vec3{0, 0, PLAYER_MOVING_FORCE*z_direction});
	this->physics_object.update(map, dt);
	this->camera.location = this->physics_object.location + this->camera.cam2world*vec3{0,CAMERA_HEIGHT,CAMERA_HORIZONTAL_DISTANCE};

	float camera_y_min = map.get_height(this->camera.location.x, this->camera.location.z) + CAMERA_MIN_HEIGHT;
	smooth_clamp_below(this->camera.location.y, camera_y_min);
}
