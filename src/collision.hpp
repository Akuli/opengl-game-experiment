#ifndef COLLISION_HPP
#define COLLISION_HPP

#include "map.hpp"
#include "physics.hpp"

bool physics_objects_collide(const PhysicsObject& a, const PhysicsObject& b, Map& map);


#endif
