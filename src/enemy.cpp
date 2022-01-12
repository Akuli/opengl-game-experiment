#include "enemy.hpp"
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <array>
#include <cmath>
#include <string>
#include <vector>
#include "camera.hpp"
#include "linalg.hpp"
#include "log.hpp"
#include "map.hpp"
#include "opengl_boilerplate.hpp"

static vec4 point_on_surface(float t, float u)
{
	using std::cos, std::sin;
	return vec4{ u*cos(t), 2*(1 - u*u) + u*u*u*0.2f*(1+sin(10*t)), u*sin(t), u };
}

static const std::vector<std::array<vec4, 3>>& get_vertex_data()
{
	static constexpr int tsteps = 150, usteps = 10;
	static std::vector<std::array<vec4, 3>> vertexdata;

	if (vertexdata.empty()) {
		float pi = std::acos(-1.0f);

		for (int tstep = 0; tstep < tsteps; tstep++) {
			for (int ustep = 0; ustep < usteps; ustep++) {
				float t1 = lerp<float>(0, 2*pi, tstep/(float)tsteps);
				float t2 = lerp<float>(0, 2*pi, (tstep+1)/(float)tsteps);
				float u1 = lerp<float>(0, 1, ustep/(float)usteps);
				float u2 = lerp<float>(0, 1, (ustep+1)/(float)usteps);

				vec4 a = point_on_surface(t1, u1);
				vec4 b = point_on_surface(t1, u2);
				vec4 c = point_on_surface(t2, u1);
				vec4 d = point_on_surface(t2, u2);

				vertexdata.push_back(std::array<vec4, 3>{a,b,c});
				vertexdata.push_back(std::array<vec4, 3>{d,b,c});
			}
		}

		// Scale it up
		for (std::array<vec4, 3>& triangle : vertexdata) {
			for (vec4& corner : triangle) {
				corner.x *= 2;
				corner.y *= 10;  // taller than wide
				corner.z *= 2;
			}
		}
	}

	return vertexdata;
}

void Enemy::render(const Camera& cam, Map& map) const
{
	vec3 location = this->physics_object.get_location();

	vec3 normal_vector = map.get_normal_vector(location.x, location.z);
	float above_floor = location.y - map.get_height(location.x, location.z);
	if (above_floor > 0) {
		// When the enemy is flying, don't follow ground shapes much
		normal_vector.y += -1 + std::exp(above_floor);
	}

	glUseProgram(this->shaderprogram);

	vec3 relative_location = location - cam.location;
	glUniform3f(
		glGetUniformLocation(this->shaderprogram, "addToLocation"),
		relative_location.x, relative_location.y, relative_location.z);

	glUniformMatrix3fv(
		glGetUniformLocation(this->shaderprogram, "world2cam"),
		1, true, &cam.world2cam.rows[0][0]);

	mat3 rotation = mat3::rotation_to_tilt_y_towards_vector(normal_vector);
	glUniformMatrix3fv(
		glGetUniformLocation(this->shaderprogram, "mapRotation"),
		1, true, &rotation.rows[0][0]);

	glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
	glDrawArrays(GL_TRIANGLES, 0, 3*get_vertex_data().size());
	glDisableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glUseProgram(0);
}

void Enemy::move_towards_player(vec3 player_location, Map& map, float dt)
{
	vec3 force = player_location - this->physics_object.get_location();
	force.y = 0;
	this->physics_object.set_extra_force(force.with_length(30));
	this->physics_object.update(map, dt);
}

Enemy::Enemy(vec3 initial_location) : physics_object{PhysicsObject(initial_location)}
{
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
		"    vertexToFragmentColor = darkerAtDistance(vec3(1,0,1)*mix(0.1, 0.4, 1-positionAndColor.w), pos);\n"
		"}\n"
		;
	this->shaderprogram = OpenglBoilerplate::create_shader_program(vertex_shader);

	const std::vector<std::array<vec4, 3>>& vertexdata = get_vertex_data();

	glGenBuffers(1, &this->vbo);
	SDL_assert(this->vbo != 0);
	glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertexdata[0])*vertexdata.size(), vertexdata.data(), GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

Enemy::~Enemy()
{
	glDeleteProgram(this->shaderprogram);
	glDeleteBuffers(1, &this->vbo);
}
