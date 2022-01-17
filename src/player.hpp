#include "physics.hpp"
#include "camera.hpp"
#include "map.hpp"

class Player {
public:
	Camera camera;
	Player(float initial_height);

	vec3 get_location() const { return this->physics_object.get_location(); }
	void render(const Camera& camera, Map& map) const;
	void move_and_turn(int z_direction, int angle_direction, Map& map, float dt);

private:
	double angle;
	PhysicsObject physics_object;
};
