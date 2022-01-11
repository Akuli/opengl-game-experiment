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
	void render(const Camera& camera);

	// Returns identity matrix, if floor is flat (get_height() returns a constant).
	// In general, returns a matrix that tilts in floor direction, mapping (0,1,0) to a normal vector.
	mat3 get_rotation_matrix(float x, float z);

private:
	std::unique_ptr<MapPrivate> priv;
};

#endif
