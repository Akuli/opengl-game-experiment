#ifndef MAP_H
#define MAP_H

#include "camera.hpp"
#include "linalg.hpp"

#include <memory>

// not gonna shit all over my h++ file with private structs and methods, sorry
struct MapPrivate;

class Map {
public:
	Map();
	~Map();
	Map(const Map&) = delete;

	float get_height(float x, float z);
	void render(const Camera& camera);

	// If the floor has constant height, returns identity matrix.
	// In general, returns a rotation that maps (0,1,0) to be perpendicular to the map.
	mat3 get_rotation_matrix(float x, float z);

private:
	std::unique_ptr<MapPrivate> priv;
};

#endif
