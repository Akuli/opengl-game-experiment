#include "camera.hpp"
#include "map.hpp"
#include "physics.hpp"

class Player {
public:
	Camera camera;
	Player(float initial_height);

	PhysicsObject physics_object;
	void move_and_turn(int z_direction, int angle_direction, Map& map, float dt);

private:
	float camera_angle;
};
