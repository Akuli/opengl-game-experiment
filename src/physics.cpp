#include "physics.hpp"
#include "log.hpp"

void PhysicsObject::update(Map& map, float dt)
{
	float map_height = map.get_height(this->location.x, this->location.z);
	vec3 normal = map.get_normal_vector(this->location.x, this->location.z);

	vec3 force = {0, -20, 0};

	if (this->location.y < map_height) {
		this->touching_ground = true;

		float friction = 1.f * (map_height - this->location.y);
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
