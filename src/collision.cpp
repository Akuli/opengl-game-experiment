#include "physics.hpp"
#include "log.hpp"
#include "misc.hpp"

static constexpr float step_goal = 1e-4f;

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
				float new_f_value = f(current + direction*new_step);
				if (new_f_value >= f_value)
					break;
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

bool physics_objects_collide(const PhysicsObject& a, const PhysicsObject& b, Map& map)
{
	if ((a.location - b.location).length_squared() > 100)
		return false;

	mat3 a_rotation = a.surface->get_rotation_matrix(map, a.location);
	mat3 b_rotation = b.surface->get_rotation_matrix(map, b.location);

	// Computes distance between two points. We want to find out if it can be zero.
	std::function<float(vec4)> function_to_minimize = [&a,&b,&map,&a_rotation,&b_rotation](vec4 input) {
		vec3 a_point_raw = a.surface->tu_to_3d_point_and_brightness(input.xy()).xyz();
		vec3 b_point_raw = b.surface->tu_to_3d_point_and_brightness(input.zw()).xyz();

		vec3 a_point = a.location + a_rotation*a_point_raw;
		vec3 b_point = b.location + b_rotation*b_point_raw;

		return (a_point - b_point).length_squared();
	};

	MinimumFinder minimum_finder = {
		a.surface->tmin, a.surface->tmax,
		a.surface->umin, a.surface->umax,
		b.surface->tmin, b.surface->tmax,
		b.surface->umin, b.surface->umax,
		function_to_minimize,
	};

	constexpr int step_count = 4;

	float minvalue = HUGE_VALF;

#define LOOP(NAME) for (int NAME = 0; NAME < step_count; NAME++)
	LOOP(xstep) LOOP(ystep) LOOP(zstep) LOOP(wstep) {
		vec4 v = {
			lerp(a.surface->tmin, a.surface->tmax, (0.5f+xstep)/step_count),
			lerp(a.surface->umin, a.surface->umax, (0.5f+ystep)/step_count),
			lerp(b.surface->tmin, b.surface->tmax, (0.5f+zstep)/step_count),
			lerp(b.surface->umin, b.surface->umax, (0.5f+wstep)/step_count),
		};
		float value = minimum_finder.find_minimum(v);
		minvalue = std::min(minvalue, value);
	}
#undef LOOP

	return (minvalue < 0.01f);
}
