#ifndef ENEMY_HPP
#define ENEMY_HPP

#include "linalg.hpp"
#include "map.hpp"
#include "entity.hpp"

class Enemy {
public:
	static void decide_location(vec3 player_location, float& x, float& z);
	Enemy(vec3 initial_location);

	Entity entity;

	void move_towards_player(vec3 player_location, Map& map, float dt);
};

#endif
