#include "enemy.h"
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <math.h>
#include "assert.h"
#include "camera.h"
#include "log.h"
#include "map.h"
#include "opengl_boilerplate.h"

struct Enemy {
	GLuint shaderprogram;
	GLuint vbo;  // Vertex Buffer Object, represents triangles going to gpu
};

static vec4 point_on_surface(float t, float u)
{
	return (vec4){ u*cosf(t), 2*(1 - u*u) + u*u*u*0.2f*(1+sinf(10*t)), u*sinf(t), u };
}

static vec4 *get_vertex_data(int *npoints)
{
#define TSTEPS 150
#define USTEPS 10
	// *4 because a rectangle is represented as 2 triangles, and clipping can split each one in half
	static vec4 vertexdata[TSTEPS*USTEPS*4][3];
	static int ntriangles = 0;

	if (ntriangles == 0) {
		float pi = acosf(-1);

		struct Plane xzplane = { .normal = {0,1,0}, .constant = 0 };

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

				ntriangles += plane_clip_triangle(xzplane, (vec4[]){a,b,c}, &vertexdata[ntriangles]);
				ntriangles += plane_clip_triangle(xzplane, (vec4[]){d,b,c}, &vertexdata[ntriangles]);
			}
		}

		// Scale it up
		for (int i = 0; i < ntriangles; i++) {
			vec4_mul_float_first3_inplace(&vertexdata[i][0], 2);
			vec4_mul_float_first3_inplace(&vertexdata[i][1], 2);
			vec4_mul_float_first3_inplace(&vertexdata[i][2], 2);
		}
	}

	*npoints = 3*ntriangles;
	return &vertexdata[0][0];
}

void enemy_render(struct Enemy *enemy, const struct Camera *cam, struct Map *map)
{
	glUseProgram(enemy->shaderprogram);

	vec3 v = vec3_sub((vec3){0,map_getheight(map, 0, 0),0}, cam->location);
	glUniform3f(
		glGetUniformLocation(enemy->shaderprogram, "addToLocation"),
		v.x, v.y, v.z);
	glUniformMatrix3fv(
		glGetUniformLocation(enemy->shaderprogram, "world2cam"),
		1, true, &cam->world2cam.rows[0][0]);
	glUniformMatrix3fv(
		glGetUniformLocation(enemy->shaderprogram, "mapRotation"),
		1, true, &map_get_rotation(map, 0, 0).rows[0][0]);

	if (enemy->vbo == 0) {
		int npoints;
		vec4 *vertexdata = get_vertex_data(&npoints);

		glGenBuffers(1, &enemy->vbo);
		SDL_assert(enemy->vbo != 0);
		glBindBuffer(GL_ARRAY_BUFFER, enemy->vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vec4)*npoints, vertexdata, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	int npoints;
	get_vertex_data(&npoints);

	glBindBuffer(GL_ARRAY_BUFFER, enemy->vbo);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
	glDrawArrays(GL_TRIANGLES, 0, npoints);
	glDisableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glUseProgram(0);
}

struct Enemy *enemy_new(void)
{
	struct Enemy *enemy = malloc(sizeof(*enemy));
	SDL_assert(enemy);
	memset(enemy, 0, sizeof *enemy);

	const char *vertex_shader =
		"#version 330\n"
		"\n"
		"layout(location = 0) in vec4 positionAndColor;\n"
		"uniform vec3 addToLocation;\n"
		"uniform mat3 world2cam;\n"
		"uniform mat3 mapRotation;\n"
		"uniform float mapHeight;\n"
		"smooth out vec4 vertexToFragmentColor;\n"
		"\n"
		"void main(void)\n"
		"{\n"
		"    vec3 pos = world2cam*(mapRotation*positionAndColor.xyz + addToLocation);\n"
		"    // Other components of (x,y,z,w) will be implicitly divided by w\n"
		"    // Resulting z will be used in z-buffer\n"
		"    gl_Position = vec4(pos.x, pos.y, 1, -pos.z);\n"
		"\n"
		"    vertexToFragmentColor.xyz = vec3(1,0,1)*mix(0.1, 0.4, 1-positionAndColor.w);\n"
		"    vertexToFragmentColor.w = 1;\n"
		"}\n"
		;
	const char *fragment_shader =
		"#version 330\n"
		"\n"
		"smooth in vec4 vertexToFragmentColor;\n"
		"out vec4 outColor;\n"
		"\n"
		"void main(void)\n"
		"{\n"
		"    outColor = vertexToFragmentColor;\n"
		"}\n"
		;
	enemy->shaderprogram = opengl_boilerplate_create_shader_program(vertex_shader, fragment_shader);

	return enemy;
}

// TODO: delete some of the opengl stuff?
void enemy_destroy(struct Enemy *enemy)
{
	free(enemy);
}
