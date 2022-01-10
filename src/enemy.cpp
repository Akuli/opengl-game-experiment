#include "enemy.hpp"
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <math.h>
#include <array>
#include <vector>
#include <stdbool.h>
#include <stddef.h>
#include "camera.hpp"
#include "linalg.hpp"
#include "map.hpp"
#include "opengl_boilerplate.hpp"

static vec4 point_on_surface(float t, float u)
{
	return vec4{ u*cosf(t), 2*(1 - u*u) + u*u*u*0.2f*(1+sinf(10*t)), u*sinf(t), u };
}

static const std::vector<std::array<vec4, 3>>& get_vertex_data()
{
	static constexpr int tsteps = 150, usteps = 10;
	static std::vector<std::array<vec4, 3>> vertexdata;

	if (vertexdata.empty()) {
		float pi = acosf(-1);

		for (int tstep = 0; tstep < tsteps; tstep++) {
			for (int ustep = 0; ustep < usteps; ustep++) {
				// Min value of t tweaked so that the tip of enemy looks good
				float t1 = lerp(0, 2*pi, tstep/(float)tsteps);
				float t2 = lerp(0, 2*pi, (tstep+1)/(float)tsteps);
				float u1 = lerp(0, 1, ustep/(float)usteps);
				float u2 = lerp(0, 1, (ustep+1)/(float)usteps);

				vec4 a = point_on_surface(t1, u1);
				vec4 b = point_on_surface(t1, u2);
				vec4 c = point_on_surface(t2, u1);
				vec4 d = point_on_surface(t2, u2);

				vertexdata.push_back(std::array<vec4, 3>{a,b,c});
				vertexdata.push_back(std::array<vec4, 3>{d,b,c});
			}
		}

		// Scale it up. No idea why won't work without indexes...
		for (int i = 0; i < vertexdata.size(); i++)
			for (int k = 0; k < vertexdata[i].size(); k++)
				vec4_mul_float_first3_inplace(&vertexdata[i][k], 2);
	}

	return vertexdata;
}

void enemy_render(const struct Enemy *en, const struct Camera *cam, struct Map *map)
{
	glUseProgram(en->shaderprogram);

	vec3 v = vec3_sub(vec3{0,map_getheight(map, 0, 0),0}, cam->location);
	glUniform3f(
		glGetUniformLocation(en->shaderprogram, "addToLocation"),
		v.x, v.y, v.z);
	glUniformMatrix3fv(
		glGetUniformLocation(en->shaderprogram, "world2cam"),
		1, true, &cam->world2cam.rows[0][0]);
	glUniformMatrix3fv(
		glGetUniformLocation(en->shaderprogram, "mapRotation"),
		1, true, (float*)map_get_rotation(map, 0, 0).rows);

	glBindBuffer(GL_ARRAY_BUFFER, en->vbo);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
	glDrawArrays(GL_TRIANGLES, 0, 3*get_vertex_data().size());
	glDisableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glUseProgram(0);
}

struct Enemy enemy_new(void)
{
	Enemy enemy;

	const char *vertex_shader =
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
	enemy.shaderprogram = opengl_boilerplate_create_shader_program(vertex_shader, NULL);

	const std::vector<std::array<vec4, 3>>& vertexdata = get_vertex_data();

	glGenBuffers(1, &enemy.vbo);
	SDL_assert(enemy.vbo != 0);
	glBindBuffer(GL_ARRAY_BUFFER, enemy.vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertexdata[0])*vertexdata.size(), vertexdata.data(), GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	return enemy;
}

void enemy_destroy(const struct Enemy *en)
{
	glDeleteProgram(en->shaderprogram);
	glDeleteBuffers(1, &en->vbo);
}
