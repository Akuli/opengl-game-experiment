#include "enemy.h"
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <math.h>
#include "camera.h"
#include "log.h"
#include "opengl_boilerplate.h"

struct Enemy {
	GLuint shaderprogram;
	GLuint vbo;  // Vertex Buffer Object, represents triangles going to gpu
};

static void create_arc_in_3d(float (*vertexdata)[12], int *ntriangles, Vec2 upper, Vec2 lower, float angle1, float angle2)
{
	float pi = acosf(-1);

	// Create n equally spaced points on outer arc
#define n 10
	Vec2 arcpoints[n];
	Vec2 center = vec2_mul_float(vec2_add(upper, lower), 0.5f);
	Vec2 center2upper = vec2_sub(upper, center);
	for (int i = 0; i < n; i++)
		arcpoints[i] = vec2_add(center, mat2_mul_vec2(mat2_rotation(-i*pi/(n-1)), center2upper));

	// Map them to 3D with two different angles
	Vec3 arcpoints1[n], arcpoints2[n];
	Mat3 rot1 = mat3_rotation_xz(angle1);
	Mat3 rot2 = mat3_rotation_xz(angle2);
	for (int i = 0; i < n; i++) {
		arcpoints1[i] = mat3_mul_vec3(rot1, (Vec3){arcpoints[i].x, arcpoints[i].y, 0});
		arcpoints2[i] = mat3_mul_vec3(rot2, (Vec3){arcpoints[i].x, arcpoints[i].y, 0});
	}

	// Connect with triangles
	for (int i = 0; i < n-1; i++) {
		Vec3 a = arcpoints1[i];
		float triangle1[] = {
			arcpoints1[i].x, arcpoints1[i].y, arcpoints1[i].z, i/(float)(n-1),
			arcpoints2[i].x, arcpoints2[i].y, arcpoints2[i].z, i/(float)(n-1),
			arcpoints1[i+1].x, arcpoints1[i+1].y, arcpoints1[i+1].z, (i+1)/(float)(n-1),
		};
		float triangle2[] = {
			arcpoints2[i+1].x, arcpoints2[i+1].y, arcpoints2[i+1].z, (i+1)/(float)(n-1),
			arcpoints2[i].x, arcpoints2[i].y, arcpoints2[i].z, i/(float)(n-1),
			arcpoints1[i+1].x, arcpoints1[i+1].y, arcpoints1[i+1].z, (i+1)/(float)(n-1),
		};
		memcpy(vertexdata[(*ntriangles)++], triangle1, sizeof triangle1);
		memcpy(vertexdata[(*ntriangles)++], triangle2, sizeof triangle2);
	}
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

	static float vertexdata[10000][12];
	int ntriangles = 0;
	float pi = acosf(-1);

#define spiral(t) ((Vec3){ (t)/10*cosf(2*pi*(t)), 1-(t)/5, (t)/10*sinf(2*pi*(t)) })

	float dt = 0.05f;

	for (float t = 0; t < 5; t += dt) {
		create_arc_in_3d(
			vertexdata, &ntriangles,
			(Vec2){t/10,1-t/5},
			(Vec2){(t+1)/10,1-(t+1)/5},
			2*pi*t, 2*pi*(t+dt));
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
		"    gl_Position = vec4(pos.x, pos.y+2, 1, -pos.z);\n"
		"\n"
		"    vertexToFragmentColor.xyz = vec3(0.5,1,0)*positionAndColor.w;\n"
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
