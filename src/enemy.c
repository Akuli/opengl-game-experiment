#include "enemy.h"
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <math.h>
#include "assert.h"
#include "camera.h"
#include "log.h"
#include "map.h"
#include "opengl_boilerplate.h"

static vec4 point_on_surface(float t, float u)
{
	return (vec4){ u*cosf(t), 2*(1 - u*u) + u*u*u*0.2f*(1+sinf(10*t)), u*sinf(t), u };
}

static vec4 *get_vertex_data(int *npoints)
{
#define TSTEPS 150
#define USTEPS 10
	static vec4 vertexdata[TSTEPS*USTEPS*2][3];
	static int ntriangles = 0;

	if (ntriangles == 0) {
		float pi = acosf(-1);

		for (int tstep = 0; tstep < TSTEPS; tstep++) {
			for (int ustep = 0; ustep < USTEPS; ustep++) {
				// Min value of t tweaked so that the tip of enemy looks good
				float t1 = lerp(0, 2*pi, tstep/(float)TSTEPS);
				float t2 = lerp(0, 2*pi, (tstep+1)/(float)TSTEPS);
				float u1 = lerp(0, 1, ustep/(float)USTEPS);
				float u2 = lerp(0, 1, (ustep+1)/(float)USTEPS);
#undef TSTEPS
#undef USTEPS

				vec4 a = point_on_surface(t1, u1);
				vec4 b = point_on_surface(t1, u2);
				vec4 c = point_on_surface(t2, u1);
				vec4 d = point_on_surface(t2, u2);

				vertexdata[ntriangles][0] = a;
				vertexdata[ntriangles][1] = b;
				vertexdata[ntriangles][2] = c;
				ntriangles++;

				vertexdata[ntriangles][0] = d;
				vertexdata[ntriangles][1] = b;
				vertexdata[ntriangles][2] = c;
				ntriangles++;
			}
		}

		// Scale it up
		for (int i = 0; i < ntriangles; i++) {
			vec4_mul_float_first3_inplace(&vertexdata[i][0], 2);
			vec4_mul_float_first3_inplace(&vertexdata[i][1], 2);
			vec4_mul_float_first3_inplace(&vertexdata[i][2], 2);
		}
	}

	SDL_assert(ntriangles == sizeof(vertexdata)/sizeof(vertexdata[0]));

	*npoints = 3*ntriangles;
	return &vertexdata[0][0];
}

void enemy_render(const struct Enemy *en, const struct Camera *cam, struct Map *map)
{
	glUseProgram(en->shaderprogram);

	vec3 v = vec3_sub((vec3){0,map_getheight(map, 0, 0),0}, cam->location);
	glUniform3f(
		glGetUniformLocation(en->shaderprogram, "addToLocation"),
		v.x, v.y, v.z);
	glUniformMatrix3fv(
		glGetUniformLocation(en->shaderprogram, "world2cam"),
		1, true, &cam->world2cam.rows[0][0]);
	glUniformMatrix3fv(
		glGetUniformLocation(en->shaderprogram, "mapRotation"),
		1, true, &map_get_rotation(map, 0, 0).rows[0][0]);

	int npoints;
	get_vertex_data(&npoints);

	glBindBuffer(GL_ARRAY_BUFFER, en->vbo);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
	glDrawArrays(GL_TRIANGLES, 0, npoints);
	glDisableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glUseProgram(0);
}

struct Enemy enemy_new(void)
{
	struct Enemy enemy = {0};

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

	int npoints;
	vec4 *vertexdata = get_vertex_data(&npoints);

	glGenBuffers(1, &enemy.vbo);
	SDL_assert(enemy.vbo != 0);
	glBindBuffer(GL_ARRAY_BUFFER, enemy.vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vec4)*npoints, vertexdata, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	return enemy;
}

void enemy_destroy(const struct Enemy *en)
{
	glDeleteProgram(en->shaderprogram);
	glDeleteBuffers(1, &en->vbo);
}
