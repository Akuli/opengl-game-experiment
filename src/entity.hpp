#ifndef ENTITY_HPP
#define ENTITY_HPP

#include <cmath>
#include "camera.hpp"  // IWYU pragma: keep
#include "linalg.hpp"
#include "map.hpp"
#include "surface.hpp"

class Entity {
public:
	Entity(Surface* surface, vec3 initial_location, float max_speed = HUGE_VALF) : location(initial_location), surface(surface), max_speed(max_speed) {}

	vec3 location;
	inline void set_extra_force(vec3 force) { this->extra_force = force; }
	void update(Map& map, float dt);

	void render(const Camera& cam, Map& map) const { this->surface->render(cam, map, this->location); }

	bool touching_ground;
	Surface* surface;  // reference caused weird compile errors elsewhere

	bool collides_with(const Entity& other, Map& map) const;

private:
	float max_speed;
	vec3 speed;
	vec3 extra_force;  // total force = gravity + extra force
};

#endif 
