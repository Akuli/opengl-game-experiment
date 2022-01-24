#ifndef SURFACE_H
#define SURFACE_H

#include <GL/glew.h>
#include <functional>
#include "camera.hpp"
#include "linalg.hpp"
#include "map.hpp"

class Surface {
public:
	std::function<vec4(float, float)> tu_to_3d_point_and_brightness;
	Surface(
		std::function<vec4(float, float)> tu_to_3d_point_and_brightness,
		float tmin, float tmax, int tstepcount,
		float umin, float umax, int ustepcount,
		float r, float g, float b);
	void render(const Camera& cam, Map& map, vec3 location);

	mat3 get_rotation_matrix(Map& map, vec3 location) const;

private:
	std::vector<std::array<vec4, 3>> vertex_data;
	void prepare_shader_program();
	GLuint shader_program;
	GLuint vertex_buffer_object;
	float r, g, b;
};

#endif
