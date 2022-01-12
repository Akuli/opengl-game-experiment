#ifndef MAP_H
#define MAP_H

#include "camera.hpp"
#include "linalg.hpp"

#include <memory>

// not gonna shit all over my h++ file with private structs and methods, sorry
struct MapPrivate;  // IWYU pragma: keep

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
	std::unique_ptr<MapPrivate> priv;
};

#endif
