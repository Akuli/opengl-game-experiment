#include "entity.hpp"
#include "config.hpp"
#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include "linalg.hpp"
#include "map.hpp"
#include "misc.hpp"
#include "surface.hpp"


void Entity::update(Map& map, float dt)
{
	float map_height = map.get_height(this->location.x, this->location.z);
	vec3 normal = map.get_normal_vector(this->location.x, this->location.z);

	vec3 force = {0, -GRAVITY, 0};

	if (this->location.y < map_height) {
		this->touching_ground = true;

		float friction = map_height - this->location.y;
		if (friction > 1)
			friction = 1;

		this->location.y = map_height;
		this->speed -= this->speed.projection_to(normal);
		this->speed *= 1 - friction;
		force += this->extra_force;
	} else {
		this->touching_ground = false;
	}

	vec3 acceleration = force;  // TODO: different masses?
	this->speed += acceleration*dt;
	this->location += speed*dt;

	if (this->speed.length() > this->max_speed)
		this->speed = this->speed.with_length(this->max_speed);
}


class MinimumFinder {
public:
	float xmin, xmax, ymin, ymax, zmin, zmax, wmin, wmax;
	std::function<float(vec4)> f;  // Finds minimum value of this function

	float find_minimum(vec4 starting_point) const
	{
		SDL_assert(this->point_is_allowed(starting_point));
		vec4 current = starting_point;

		int iter = 0;
		while(1) {
			std::optional<vec4> direction = this->find_direction(current);
			if (!direction)
				return f(current);

			float step = find_step_size(current, direction.value());
			if (step < step_goal || iter++ == 10)
				return f(current);
			current += direction.value()*step;
		}
	}

private:
	static constexpr float step_goal = 1e-4f;

	bool point_is_allowed(vec4 v) const
	{
		return xmin<v.x && v.x<xmax
			&& ymin<v.y && v.y<ymax
			&& zmin<v.z && v.z<zmax
			&& wmin<v.w && v.w<wmax;
	}

	float find_step_size(vec4 current, vec4 direction) const
	{
		float step = step_goal/2;
		if (!this->point_is_allowed(current + direction*step))
			return 0;

		float ratios[] = { 2.0f, 1.1f }; // First find about the right size, then refine

		float f_value = f(current + direction*step);
		for (float r : ratios) {
			while(1) {
				float new_step = step*r;
				float new_f_value;
				if (!this->point_is_allowed(current + direction*new_step) ||
					(new_f_value = f(current + direction*new_step)) >= f_value)
				{
					break;
				}
				step = new_step;
				f_value = new_f_value;
			}
		}

		return step;
	}

	std::optional<vec4> find_direction(vec4 current) const
	{
		float h = 1e-5f;
		float fcur = f(current);
		vec4 gradient = {
			(f(current + vec4(h,0,0,0)) - fcur)/h,
			(f(current + vec4(0,h,0,0)) - fcur)/h,
			(f(current + vec4(0,0,h,0)) - fcur)/h,
			(f(current + vec4(0,0,0,h)) - fcur)/h,
		};

		if (gradient.length_squared() < 1e-3f)
			return std::nullopt;
		return gradient * (-1.0f/gradient.length());
	}
};

bool Entity::collides_with(const Entity& other, Map& map) const
{
	mat3 this_rotation = this->surface->get_rotation_matrix(map, this->location);
	mat3 other_rotation = other.surface->get_rotation_matrix(map, other.location);

	// Computes distance between two points. We want to find out if it can be zero.
	std::function<float(vec4)> function_to_minimize
	= [this,&other,&map,&this_rotation,&other_rotation](vec4 input)
	{
		vec3 this_point_raw = this->surface->tu_to_3d_point_and_brightness(input.xy()).xyz();
		vec3 other_point_raw = other.surface->tu_to_3d_point_and_brightness(input.zw()).xyz();

		vec3 this_point = this->location + this_rotation*this_point_raw;
		vec3 other_point = other.location + other_rotation*other_point_raw;

		return (this_point - other_point).length_squared();
	};

	MinimumFinder minimum_finder = {
		this->surface->tmin, this->surface->tmax,
		this->surface->umin, this->surface->umax,
		other.surface->tmin, other.surface->tmax,
		other.surface->umin, other.surface->umax,
		function_to_minimize,
	};

	// Don't make this too big, run time is proportional to step_count^4
	constexpr int step_count = 4;

	float minvalue = HUGE_VALF;

#define LOOP(NAME) for (int NAME = 0; NAME < step_count; NAME++)
	LOOP(xstep) LOOP(ystep) LOOP(zstep) LOOP(wstep) {
		vec4 v = {
			lerp(this->surface->tmin, this->surface->tmax, (0.5f+xstep)/step_count),
			lerp(this->surface->umin, this->surface->umax, (0.5f+ystep)/step_count),
			lerp(other.surface->tmin, other.surface->tmax, (0.5f+zstep)/step_count),
			lerp(other.surface->umin, other.surface->umax, (0.5f+wstep)/step_count),
		};
		float value = minimum_finder.find_minimum(v);
		minvalue = std::min(minvalue, value);
	}
#undef LOOP

	return (minvalue < 0.01f);
}
