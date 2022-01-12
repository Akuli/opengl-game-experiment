#ifndef PHYSICS_H
#define PHYSICS_H

#include "linalg.hpp"
#include "map.hpp"

class PhysicsObject {
public:
	PhysicsObject(vec3 initial_location) : location(initial_location) {}
	PhysicsObject(const PhysicsObject&) = delete;
	inline vec3 get_location() const { return this->location; }
	inline void set_extra_force(vec3 force) { this->extra_force = force; }
	void update(Map& map, float dt);
	bool touching_ground;

private:
	vec3 location;
	vec3 speed;
	vec3 extra_force;  // total force = gravity + extra force
};

#endif 
