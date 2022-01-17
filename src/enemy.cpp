#include "enemy.hpp"
#include <functional>
#include "camera.hpp"
#include "config.hpp"
#include "linalg.hpp"
#include "map.hpp"
#include "misc.hpp"
#include "surface.hpp"

Enemy::Enemy(vec3 initial_location) : physics_object{PhysicsObject(initial_location, ENEMY_MAX_SPEED)}
{ }

static vec4 tu_to_3d_point_and_brightness(float t, float u)
{
	using std::cos, std::sin;
	return vec4{ 2*u*cos(t), 6*(1 - u*u) + 0.6f*u*u*u*(1+sin(10*t)), 2*u*sin(t), lerp<float>(0.1f, 0.4f, 1-u) };
}

void Enemy::render(const Camera& camera, Map& map) const
{
	float pi = std::acos(-1.0f);
	static Surface surface = Surface(tu_to_3d_point_and_brightness,
		0, 2*pi, 150,
		0, 1, 10,
		1.0f, 0.0f, 1.0f);
	surface.render(camera, map, this->get_location());
}

void Enemy::move_towards_player(vec3 player_location, Map& map, float dt)
{
	vec3 force = player_location - this->physics_object.get_location();
	force.y = 0;
	this->physics_object.set_extra_force(force.with_length(ENEMY_MOVING_FORCE));
	this->physics_object.update(map, dt);
}

void Enemy::decide_location(vec3 player_location, float& x, float& z)
{
	float pi = std::acos(-1.0f);
	float angle = uniform_random_float(0, 2*pi);

	x = player_location.x + VIEW_RADIUS*std::cos(angle);
	z = player_location.z + VIEW_RADIUS*std::sin(angle);
}
