#include <SDL2/SDL.h>
#include <GL/glew.h>
#include "config.hpp"
#include "physics.hpp"
#include "camera.hpp"
#include "player.hpp"
#include <string>
#include "opengl_boilerplate.hpp"
#include "log.hpp"
#include "misc.hpp"

Player::Player(float initial_height) : physics_object(vec3(0,initial_height,0)) {}

static vec4 point_on_surface(float t, float u)
{
	using std::cos, std::sin;
	return vec4{ u*cos(t), 2*(1 - u*u) + u*u*u*0.2f*(1+sin(10*t)), u*sin(t), u };
}

static std::vector<std::array<vec4, 3>> generate_vertex_data()
{
	static constexpr int tsteps = 150, usteps = 10;

	std::vector<std::array<vec4, 3>> vertex_data = {};
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

			vertex_data.push_back(std::array<vec4, 3>{a,b,c});
			vertex_data.push_back(std::array<vec4, 3>{d,b,c});
		}
	}

	// Scale it up
	for (std::array<vec4, 3>& triangle : vertex_data) {
		for (vec4& corner : triangle) {
			corner.x *= 2;
			corner.y *= 3;  // taller than wide
			corner.z *= 2;
		}
	}

	return vertex_data;
}

// vbo = Vertex Buffer Object, represents triangles going to gpu
static void initialize_rendering(GLuint& shader_program_out, GLuint& vbo_out, int& triangle_count_out)
{
	static GLuint shader_program, vbo;
	static int triangle_count = -1;

	if (triangle_count == -1) {
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
		shader_program = OpenglBoilerplate::create_shader_program(vertex_shader);

		const std::vector<std::array<vec4, 3>>& vertexdata = generate_vertex_data();
		triangle_count = vertexdata.size();

		glGenBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertexdata[0])*vertexdata.size(), vertexdata.data(), GL_DYNAMIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	shader_program_out = shader_program;
	vbo_out = vbo;
	triangle_count_out = triangle_count;
}

void Player::render(const Camera& cam, Map& map) const
{
	vec3 location = this->physics_object.get_location();

	vec3 normal_vector = map.get_normal_vector(location.x, location.z);
	float above_floor = location.y - map.get_height(location.x, location.z);
	if (above_floor > 0) {
		// When the enemy is flying, don't follow ground shapes much
		normal_vector /= normal_vector.length();
		normal_vector.y += above_floor*above_floor*0.2f;
	}

	GLuint shader_program, vbo;
	int triangle_count;
	initialize_rendering(shader_program, vbo, triangle_count);

	glUseProgram(shader_program);

	vec3 relative_location = location - cam.location;
	glUniform3f(
		glGetUniformLocation(shader_program, "addToLocation"),
		relative_location.x, relative_location.y, relative_location.z);

	glUniformMatrix3fv(
		glGetUniformLocation(shader_program, "world2cam"),
		1, true, &cam.world2cam.rows[0][0]);

	mat3 rotation = mat3::rotation_to_tilt_y_towards_vector(normal_vector);
	glUniformMatrix3fv(
		glGetUniformLocation(shader_program, "mapRotation"),
		1, true, &rotation.rows[0][0]);

	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
	glDrawArrays(GL_TRIANGLES, 0, 3*triangle_count);
	glDisableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glUseProgram(0);
}

void Player::move_and_turn(int z_direction, int angle_direction, Map& map, float dt)
{
	SDL_assert(z_direction == 0 || z_direction == -1 || z_direction == 1);
	SDL_assert(angle_direction == 0 || angle_direction == -1 || angle_direction == 1);

	this->camera_angle += PLAYER_TURNING_SPEED*dt*angle_direction;
	this->camera.cam2world = mat3::rotation_about_y(this->camera_angle);
	this->camera.world2cam = mat3::rotation_about_y(-this->camera_angle);

	this->physics_object.set_extra_force(this->camera.cam2world * vec3{0, 0, PLAYER_MOVING_FORCE*z_direction});
	this->physics_object.update(map, dt);
	this->camera.location = this->get_location() + this->camera.cam2world*vec3{0,CAMERA_HEIGHT,CAMERA_HORIZONTAL_DISTANCE};
}
