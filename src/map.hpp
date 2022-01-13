#ifndef MAP_H
#define MAP_H

#include "camera.hpp"
#include "linalg.hpp"

#include <memory>
#include <vector>
#include <unordered_map>

namespace MapPrivate {
	struct IntPairHasher { size_t operator() (const std::pair<int,int> &pair) const; };
	extern const int section_size;

	// not gonna shit all over my h++ file with private structs and methods, sorry
	struct MapData;
}


// Partitions the world into sections to make finding nearby objects easier
class Enemy;  // FIXME: project structure = shit
class SectionedStorage
{
public:
	// TODO: don't return a vector, some kind of iterator instead?
	std::vector<Enemy*> find_within_circle(float center_x, float center_z, float radius) const;
	void add_object(Enemy&& object);

	int size() const { return this->count; }

private:
	int count;
	std::unordered_map<std::pair<int, int>, std::vector<Enemy>, MapPrivate::IntPairHasher> objects;
};


class Map {
public:
	Map();
	~Map();
	Map(const Map&) = delete;

	// Methods are not const, because map may be expanded dynamically when they are called

	float get_height(float x, float z);
	vec3 get_normal_vector(float x, float z);  // arbitrary length, points away from surface
	void render(const Camera& camera);

private:
	std::unique_ptr<MapPrivate::MapData> priv;
};

#endif
