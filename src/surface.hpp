#ifndef SURFACE_H
#define SURFACE_H

#include <functional>
#include <GL/glew.h>
#include "camera.hpp"
#include "map.hpp"

class Surface {
public:
	Surface(
		std::function<vec4(float, float)> tu_to_3d_point_and_brightness,
		float tmin, float tmax, int tstepcount,
		float umin, float umax, int ustepcount,
		float r, float g, float b);
	void render(const Camera& cam, Map& map, vec3 location);

private:
	GLuint shader_program;
	GLuint vertex_buffer_object;
	int triangle_count;
};

#endif
