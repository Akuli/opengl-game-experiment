#include "enemy.h"
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <math.h>
#include "assert.h"
#include "camera.h"
#include "log.h"
#include "opengl_boilerplate.h"

struct Enemy {
	GLuint shaderprogram;
	GLuint vbo;  // Vertex Buffer Object, represents triangles going to gpu
};

static vec4 point_on_surface(float t, float u)
{
	int n = 5;  // How many "layers" should the poo have
	float pi = acosf(-1);
	float angle = n*2*pi*t;
	float colorval = u/pi;

#define y(t) (2-(t)-(t)*(t))
	vec2 arccenter = { t + 0.5f/n, (y(t)+y(t+1.0f/n))/2 };
	vec2 arcdiff = { -0.5f/n, (y(t)-y(t+1.0f/n))/2 };
#undef y

	vec2 arcpoint = vec2_add(arccenter, mat2_mul_vec2(mat2_rotation(-u), arcdiff));
	vec3 arcpoint3d = mat3_mul_vec3(mat3_rotation_xz(angle), (vec3){ arcpoint.x, arcpoint.y, 0 });
	return (vec4){ arcpoint3d.x, arcpoint3d.y, arcpoint3d.z, colorval };
}

void enemy_render(struct Enemy *enemy, const struct Camera *cam)
{
	glUseProgram(enemy->shaderprogram);
	glUniform3f(
		glGetUniformLocation(enemy->shaderprogram, "cameraLocation"),
		cam->location.x, cam->location.y, cam->location.z);
	glUniformMatrix3fv(
		glGetUniformLocation(enemy->shaderprogram, "world2cam"),
		1, true, &cam->world2cam.rows[0][0]);

	static vec4 vertexdata[10000][3];
	int ntriangles = 0;
	float pi = acosf(-1);

	int tsteps = 100, usteps = 10;
	struct Plane xzplane = { .normal = {0,1,0}, .constant = 0 };

	for (int tstep = 0; tstep < tsteps; tstep++) {
		for (int ustep = 0; ustep < usteps; ustep++) {
			// Min value of t tweaked so that the tip of enemy looks good
			float t1 = lerp(-0.1f, 1, tstep/(float)tsteps);
			float t2 = lerp(-0.1f, 1, (tstep+1)/(float)tsteps);
			float u1 = lerp(0, pi, ustep/(float)usteps);
			float u2 = lerp(0, pi, (ustep+1)/(float)usteps);

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

	if (enemy->vbo == 0) {
		glGenBuffers(1, &enemy->vbo);
		SDL_assert(enemy->vbo != 0);
		glBindBuffer(GL_ARRAY_BUFFER, enemy->vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertexdata), NULL, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	glBindBuffer(GL_ARRAY_BUFFER, enemy->vbo);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertexdata), vertexdata);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
	glDrawArrays(GL_TRIANGLES, 0, 4*ntriangles);
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
		"uniform vec3 cameraLocation;\n"
		"uniform mat3 world2cam;\n"
		"smooth out vec4 vertexToFragmentColor;\n"
		"\n"
		"void main(void)\n"
		"{\n"
		"    vec3 pos = world2cam*(positionAndColor.xyz - cameraLocation);\n"
		"    // Other components of (x,y,z,w) will be implicitly divided by w\n"
		"    // Resulting z will be used in z-buffer\n"
		"    gl_Position = vec4(pos.x, pos.y+1, 1, -pos.z);\n"
		"\n"
		"    vertexToFragmentColor.xyz = mix(vec3(0.6,0.3,0), vec3(0,0,0), positionAndColor.w);\n"
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
