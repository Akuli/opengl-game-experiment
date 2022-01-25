#include "camera.hpp"
#include "map.hpp"
#include "entity.hpp"

class Player {
public:
	Camera camera;
	Player(float initial_height);

	Entity entity;
	void move_and_turn(int z_direction, int angle_direction, Map& map, float dt);

private:
	float camera_angle;
};
