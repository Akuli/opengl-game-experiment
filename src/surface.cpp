#include "surface.hpp"
#include <array>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>
#include "camera.hpp"
#include "map.hpp"
#include "misc.hpp"
#include "opengl_boilerplate.hpp"


static std::vector<std::array<vec4, 3>> create_vertex_data(
	std::function<vec4(float, float)> tu_to_3d_point_and_brightness,
	float tmin, float tmax, int tstepcount,
	float umin, float umax, int ustepcount)
{
	std::vector<std::array<vec4, 3>> vertex_data = {};
	for (int tstep = 0; tstep < tstepcount; tstep++) {
		for (int ustep = 0; ustep < ustepcount; ustep++) {
			float t1 = lerp<float>(tmin, tmax,  tstep   /float(tstepcount));
			float t2 = lerp<float>(tmin, tmax, (tstep+1)/float(tstepcount));
			float u1 = lerp<float>(umin, umax,  ustep   /float(ustepcount));
			float u2 = lerp<float>(umin, umax, (ustep+1)/float(ustepcount));

			vec4 a = tu_to_3d_point_and_brightness(t1, u1);
			vec4 b = tu_to_3d_point_and_brightness(t1, u2);
			vec4 c = tu_to_3d_point_and_brightness(t2, u1);
			vec4 d = tu_to_3d_point_and_brightness(t2, u2);

			vertex_data.push_back(std::array<vec4, 3>{a,b,c});
			vertex_data.push_back(std::array<vec4, 3>{d,b,c});
		}
	}
	return vertex_data;
}

Surface::Surface(
	std::function<vec4(float, float)> tu_to_3d_point_and_brightness,
	float tmin, float tmax, int tstepcount,
	float umin, float umax, int ustepcount,
	float r, float g, float b)
{
	std::vector<std::array<vec4, 3>> vertex_data = create_vertex_data(
		tu_to_3d_point_and_brightness,
		tmin, tmax, tstepcount,
		umin, umax, ustepcount);

	this->triangle_count = vertex_data.size();

	// TODO: use a uniform
	char rgb_string[500];
	std::sprintf(rgb_string, "vec3(%f,%f,%f)", r, g, b);

	std::string vertex_shader =
		"#version 330\n"
		"\n"
		"layout(location = 0) in vec4 positionAndColor;\n"
		"uniform vec3 addToLocation;\n"
		"uniform mat3 world2cam;\n"
		"uniform mat3 mapRotation;\n"
		"smooth out vec4 vertexToFragmentColor;\n"
		"\n"
		"BOILERPLATE_GOES_HERE\n"
		"\n"
		"void main(void)\n"
		"{\n"
		"    vec3 pos = world2cam*(mapRotation*positionAndColor.xyz + addToLocation);\n"
		"    gl_Position = locationFromCameraToGlPosition(pos);\n"
		"    vertexToFragmentColor = darkerAtDistance(vec3("+std::string(rgb_string)+")*positionAndColor.w, pos);\n"
		"}\n"
		;
	this->shader_program = OpenglBoilerplate::create_shader_program(vertex_shader);

	glGenBuffers(1, &this->vertex_buffer_object);
	glBindBuffer(GL_ARRAY_BUFFER, this->vertex_buffer_object);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data[0])*vertex_data.size(), vertex_data.data(), GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Surface::render(const Camera& cam, Map& map, vec3 location)
{
	vec3 normal_vector = map.get_normal_vector(location.x, location.z);
	float above_floor = location.y - map.get_height(location.x, location.z);
	if (above_floor > 0) {
		// When the player or enemy is flying, don't follow ground shapes much
		normal_vector /= normal_vector.length();
		normal_vector.y += above_floor*above_floor*0.2f;
	}

	glUseProgram(this->shader_program);

	vec3 relative_location = location - cam.location;
	glUniform3f(
		glGetUniformLocation(this->shader_program, "addToLocation"),
		relative_location.x, relative_location.y, relative_location.z);

	glUniformMatrix3fv(
		glGetUniformLocation(this->shader_program, "world2cam"),
		1, true, &cam.world2cam.rows[0][0]);

	mat3 rotation = mat3::rotation_to_tilt_y_towards_vector(normal_vector);
	glUniformMatrix3fv(
		glGetUniformLocation(this->shader_program, "mapRotation"),
		1, true, &rotation.rows[0][0]);

	glBindBuffer(GL_ARRAY_BUFFER, this->vertex_buffer_object);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
	glDrawArrays(GL_TRIANGLES, 0, 3*this->triangle_count);
	glDisableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glUseProgram(0);
}
