#ifndef PHYSICS_H
#define PHYSICS_H

#include <cmath>
#include "linalg.hpp"
#include "map.hpp"

class PhysicsObject {
public:
	PhysicsObject(vec3 initial_location, float max_speed = HUGE_VALF) : max_speed(max_speed), location(initial_location) {}
	PhysicsObject(const PhysicsObject&) = delete;
	PhysicsObject(PhysicsObject&&) = default;

	inline vec3 get_location() const { return this->location; }
	inline void set_extra_force(vec3 force) { this->extra_force = force; }
	void update(Map& map, float dt);
	bool touching_ground;

private:
	float max_speed;
	vec3 location;
	vec3 speed;
	vec3 extra_force;  // total force = gravity + extra force
};

#endif 
