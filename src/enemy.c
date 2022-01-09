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

#define N_ARC_POINTS 10

// Create equally spaced points on outer arc connecting two points in 2D
static void create_arc_points(Vec2 *res, Vec2 upper, Vec2 lower)
{
	Vec2 center = vec2_mul_float(vec2_add(upper, lower), 0.5f);
	Vec2 center2upper = vec2_sub(upper, center);
	float pi = acosf(-1);
	for (int i = 0; i < N_ARC_POINTS; i++)
		res[i] = vec2_add(center, mat2_mul_vec2(mat2_rotation(-i*pi/(N_ARC_POINTS-1)), center2upper));
}

void swap4(float *a, float *b)
{
	float tmp[4];
	memcpy(tmp, a, sizeof tmp);
	memcpy(a, b, sizeof tmp);
	memcpy(b, tmp, sizeof tmp);
}

// triangle contains 12 floats, x,y,z,color for each vertex
static void add_triangle_clipped_above_xz_plane(float (*vertexdata)[12], int *ntriangles, const float *newvertexdata)
{
	float triangle[3][4];
	static_assert(sizeof(triangle) == sizeof(vertexdata[0]), "");
	memcpy(triangle, newvertexdata, sizeof triangle);
#define x(point) point[0]
#define y(point) point[1]
#define z(point) point[2]
#define w(point) point[3]  // color value

	// Sort by y, lowest first
	if (y(triangle[1]) < y(triangle[0])) swap4(triangle[1], triangle[0]);
	if (y(triangle[2]) < y(triangle[1])) swap4(triangle[2], triangle[1]);
	if (y(triangle[1]) < y(triangle[0])) swap4(triangle[1], triangle[0]);
	SDL_assert(y(triangle[0]) <= y(triangle[1]) && y(triangle[1]) <= y(triangle[2]));

	if (y(triangle[2]) <= 0) {
		// all below
		return;
	}
	if (y(triangle[0]) >= 0) {
		// all above
		memcpy(&vertexdata[(*ntriangles)++], triangle, sizeof triangle);
		return;
	}

	if (y(triangle[1]) < 0) {
		// 1 corner above, 2 below. Slide bottom 2 corners up along the sides of triangle.
		for (int i = 0; i < 2; i++) {
			float t = y(triangle[i]) / (y(triangle[i]) - y(triangle[2]));
			x(triangle[i]) += t*(x(triangle[2]) - x(triangle[i]));
			y(triangle[i]) += t*(y(triangle[2]) - y(triangle[i]));
			z(triangle[i]) += t*(z(triangle[2]) - z(triangle[i]));
		}
		memcpy(&vertexdata[(*ntriangles)++], triangle, sizeof triangle);
	} else {
		/*
		2 corners above, 1 below. Split into two triangles

			triangle[1]       triangle[2]
                \             .-'/
                 \         .-'  /
                  \     .-'    /
                   \ .-'      /
			---------------------------------- y=0
			         \      /
			          \    /
			       triangle[0]
		*/
		Vec3 v0 = { x(triangle[0]), y(triangle[0]), z(triangle[0]) };
		Vec3 v1 = { x(triangle[1]), y(triangle[1]), z(triangle[1]) };
		Vec3 v2 = { x(triangle[2]), y(triangle[2]), z(triangle[2]) };
		Vec3 bot1 = vec3_sub(v1, vec3_mul_float(vec3_sub(v0,v1), v1.y/(v0.y-v1.y)));
		Vec3 bot2 = vec3_sub(v2, vec3_mul_float(vec3_sub(v0,v2), v2.y/(v0.y-v2.y)));
		float bot1color = w(triangle[1]) - (w(triangle[0])-w(triangle[1]))*v1.y/(v0.y-v1.y);
		float bot2color = w(triangle[2]) - (w(triangle[0])-w(triangle[2]))*v2.y/(v0.y-v2.y);
		float triangle1[] = {
			v1.x, v1.y, v1.z, w(triangle[1]),
			v2.x, v2.y, v2.z, w(triangle[2]),
			bot1.x, bot1.y, bot1.z, bot1color,
		};
		float triangle2[] = {
			bot2.x, bot2.y, bot2.z, bot2color,
			v2.x, v2.y, v2.z, w(triangle[2]),
			bot1.x, bot1.y, bot1.z, bot1color,
		};
		memcpy(&vertexdata[(*ntriangles)++], triangle1, sizeof triangle1);
		memcpy(&vertexdata[(*ntriangles)++], triangle2, sizeof triangle2);
	}

#undef x
#undef y
#undef z
#undef w
}

static void create_arc_in_3d(
	float (*vertexdata)[12], int *ntriangles,
	Vec2 upper1, Vec2 lower1, float angle1,
	Vec2 upper2, Vec2 lower2, float angle2)
{
	Vec2 arcpoints1_2d[N_ARC_POINTS], arcpoints2_2d[N_ARC_POINTS];
	create_arc_points(arcpoints1_2d, upper1, lower1);
	create_arc_points(arcpoints2_2d, upper2, lower2);

	// Map arc points to 3D
	Vec3 arcpoints1[N_ARC_POINTS], arcpoints2[N_ARC_POINTS];
	Mat3 rot1 = mat3_rotation_xz(angle1);
	Mat3 rot2 = mat3_rotation_xz(angle2);
	for (int i = 0; i < N_ARC_POINTS; i++) {
		arcpoints1[i] = mat3_mul_vec3(rot1, (Vec3){arcpoints1_2d[i].x, arcpoints1_2d[i].y, 0});
		arcpoints2[i] = mat3_mul_vec3(rot2, (Vec3){arcpoints2_2d[i].x, arcpoints2_2d[i].y, 0});
	}

	// Connect with triangles
	for (int i = 0; i < N_ARC_POINTS-1; i++) {
		float triangle1[] = {
			arcpoints1[i].x, arcpoints1[i].y, arcpoints1[i].z, i/(float)(N_ARC_POINTS-1),
			arcpoints2[i].x, arcpoints2[i].y, arcpoints2[i].z, i/(float)(N_ARC_POINTS-1),
			arcpoints1[i+1].x, arcpoints1[i+1].y, arcpoints1[i+1].z, (i+1)/(float)(N_ARC_POINTS-1),
		};
		float triangle2[] = {
			arcpoints2[i+1].x, arcpoints2[i+1].y, arcpoints2[i+1].z, (i+1)/(float)(N_ARC_POINTS-1),
			arcpoints2[i].x, arcpoints2[i].y, arcpoints2[i].z, i/(float)(N_ARC_POINTS-1),
			arcpoints1[i+1].x, arcpoints1[i+1].y, arcpoints1[i+1].z, (i+1)/(float)(N_ARC_POINTS-1),
		};
		add_triangle_clipped_above_xz_plane(vertexdata, ntriangles, triangle1);
		add_triangle_clipped_above_xz_plane(vertexdata, ntriangles, triangle2);
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
	int n = 5;
	float dt = 0.05f / n;

	/*
	Spiral equation, when viewed from the side, with x axis spinning about the z axis:

		0 <= x <= 1
		y = quadratic function of x, chosen with experimentation

	The spinning speed is constant, chosen so that we do a total of n turns as t grows.
	*/
#define outer_shape_in_2d(t) ((Vec2){ (t), 2 - (t) - (t)*(t) })
	for (float t = 0; t < 1; t += dt) {
		create_arc_in_3d(
			vertexdata, &ntriangles,
			outer_shape_in_2d(t), outer_shape_in_2d(t+1.0f/n), n*2*pi*t,
			outer_shape_in_2d(t+dt), outer_shape_in_2d(t+dt+1.0f/n), n*2*pi*(t+dt));
	}
#undef outer_shape_in_2d

	// Scale it up
	for (int i = 0; i < 3*ntriangles; i++) {
		((float*)vertexdata)[4*i] *= 2;
		((float*)vertexdata)[4*i+1] *= 2;
		((float*)vertexdata)[4*i+2] *= 2;
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
