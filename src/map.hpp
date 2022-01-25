#ifndef MAP_HPP
#define MAP_HPP

#include <memory>
#include <vector>
#include "camera.hpp"
#include "linalg.hpp"

class Enemy;  // IWYU pragma: keep  // FIXME: project structure = shit
class Entity;  // IWYU pragma: keep  // FIXME: project structure = shit
struct MapPrivate;  // IWYU pragma: keep  // don't want to shit private stuff all over header file

class Map {
public:
	Map();
	~Map();
	Map(const Map&) = delete;

	// Methods are not const, because map may be expanded dynamically when they are called

	float get_height(float x, float z);
	vec3 get_normal_vector(float x, float z);  // arbitrary length, points away from surface
	void render(const Camera& camera);

	void add_enemy(const Enemy&);
	void move_enemies(vec3 player_location, float dt);
	int get_number_of_enemies() const;

	// TODO: don't return a vector, some kind of iterator instead?
	std::vector<const Enemy*> find_enemies_within_circle(float center_x, float center_z, float radius) const;
	std::vector<const Enemy*> find_colliding_enemies(const Entity& collide_with);

	void remove_enemies(const std::vector<const Enemy *> enemies);

private:
	std::unique_ptr<MapPrivate> priv;
};

#endif
