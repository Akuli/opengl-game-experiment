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

void enemy_render(struct Enemy *enemy, const struct Camera *cam)
{
	glUseProgram(enemy->shaderprogram);
	glUniform3f(
		glGetUniformLocation(enemy->shaderprogram, "cameraLocation"),
		cam->location.x, cam->location.y, cam->location.z);
	glUniformMatrix3fv(
		glGetUniformLocation(enemy->shaderprogram, "world2cam"),
		1, true, &cam->world2cam.rows[0][0]);

	float vertexdata[] = {
		1,5,0,
		2,5,0,
		1,6,0,
	};

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
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glDrawArrays(GL_TRIANGLES, 0, 3);  // 3 = 3*1 = 3*(number of triangles)
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
		"layout(location = 0) in vec3 position;\n"
		"uniform vec3 cameraLocation;\n"
		"uniform mat3 world2cam;\n"
		"smooth out vec4 vertexToFragmentColor;\n"
		"\n"
		"void main(void)\n"
		"{\n"
		"    vec3 pos = world2cam*(position - cameraLocation);\n"
		"    // Other components of (x,y,z,w) will be implicitly divided by w\n"
		"    // Resulting z will be used in z-buffer\n"
		"    gl_Position = vec4(pos.x, pos.y, 1, -pos.z);\n"
		"\n"
		"    vertexToFragmentColor.xyz = vec3(1, 0.5, 0) * 0.3;\n"
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
