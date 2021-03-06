#include "enemy.hpp"
#include <functional>
#include "config.hpp"
#include "linalg.hpp"
#include "map.hpp"
#include "misc.hpp"
#include "surface.hpp"

static vec4 tu_to_3d_point_and_brightness(vec2 tu)
{
	using std::cos, std::sin;
	float t = tu.x, u = tu.y;
	return vec4{ 2*u*cos(t), 6*(1 - u*u) + 0.6f*u*u*u*(1+sin(10*t)), 2*u*sin(t), lerp<float>(0.1f, 0.4f, 1-u) };
}

static Surface surface = Surface(
	tu_to_3d_point_and_brightness,
	0, 2*std::acos(-1.0f), 150,
	0, 1, 10,
	1.0f, 0.0f, 1.0f);

Enemy::Enemy(vec3 initial_location) : entity{Entity(&surface, initial_location, ENEMY_MAX_SPEED)}
{ }

void Enemy::move_towards_player(vec3 player_location, Map& map, float dt)
{
	vec3 force = player_location - this->entity.location;
	force.y = 0;
	this->entity.set_extra_force(force.with_length(ENEMY_MOVING_FORCE));
	this->entity.update(map, dt);
}

void Enemy::decide_location(vec3 player_location, float& x, float& z)
{
	float pi = std::acos(-1.0f);
	float angle = uniform_random_float(0, 2*pi);

	x = player_location.x + VIEW_RADIUS*std::cos(angle);
	z = player_location.z + VIEW_RADIUS*std::sin(angle);
}
