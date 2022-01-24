#ifndef COLLISION_HPP
#define COLLISION_HPP

#include "physics.hpp"

static vec3 get_xyz_from_vec4(vec4 v)
{
	return vec3{ v.x, v.y, v.z };
}

bool physics_objects_collide(const PhysicsObject& a, const PhysicsObject& b)
{
	if ((a.location - b.location).length_squared() > 100)
		return false;

	// Computes distance between two points
	std::function<float(vec4)> function_to_minimize = [a,b](vec4 input) {
		vec2 a_input = { input.x, input.y };
		vec2 b_input = { input.z, input.w };

		vec3 a_point_relative_to_location = get_xyz_from_vec4(a.surface->tu_to_3d_point_and_brightness(a_input.x, a_input.y));
		vec3 b_point_relative_to_location = get_xyz_from_vec4(a.surface->tu_to_3d_point_and_brightness(a_input.x, a_input.y));

		

		return 12.34f;
	};

	// FIXME
	return true;
}


#endif
