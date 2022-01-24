#ifndef ENEMY_H
#define ENEMY_H

#include "camera.hpp"
#include "linalg.hpp"
#include "map.hpp"
#include "physics.hpp"

class Enemy {
public:
	static void decide_location(vec3 player_location, float& x, float& z);
	Enemy(vec3 initial_location);
	Enemy& operator=(const Enemy&) = default;

	PhysicsObject physics_object;

	void move_towards_player(vec3 player_location, Map& map, float dt);
};

#endif
