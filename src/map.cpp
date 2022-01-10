#include "map.hpp"
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <array>
#include <random>
#include "camera.hpp"
#include "log.hpp"
#include "opengl_boilerplate.hpp"

#define min(x, y) ((x)<(y) ? (x) : (y))
#define min4(a,b,c,d) min(min(a,b),min(c,d))

#define SECTION_SIZE 40  // side length of section square on xz plane
#define TRIANGLES_PER_SECTION (2*SECTION_SIZE*SECTION_SIZE)

struct GaussianCurveMountain {
	float xzscale, yscale, centerx, centerz;
	GaussianCurveMountain() = default;
	GaussianCurveMountain(float xzscale, float yscale, float centerx, float centerz) : xzscale(xzscale),yscale(yscale),centerx(centerx),centerz(centerz) {}
};

struct Section {
	int startx, startz;

	/*
	y = yscale*e^(-(((x - centerx) / xzscale)^2 + ((z - centerz) / xzscale)^2))
	yscale can be negative, xzscale can't
	center coords are within the section and relative to section start, not depending on location of section
	*/
	std::array<GaussianCurveMountain, 100> mountains;

	/*
	y_table contains cached values for height of map, depending also on neighbor sections.

	Raw version:
		- contains enough values to cover neighbors too
		- does not take in account neighbors
		- is slow to compute
		- is always ready to be used, even when y_table_and_vertexdata_ready is false

	vertexdata is passed to the gpu for rendering, and represents triangles.
	*/
	std::array<std::array<float, 3*SECTION_SIZE + 1>, 3*SECTION_SIZE + 1> raw_y_table;
	std::array<std::array<float, SECTION_SIZE + 1>, SECTION_SIZE + 1> y_table;
	std::array<std::array<vec3, 3>, TRIANGLES_PER_SECTION> vertexdata;
	bool y_table_and_vertexdata_ready;
};

static float uniform_random_float(float min, float max)
{
	static std::default_random_engine random_engine = {};
	static bool ready = false;
	if (!ready)
		random_engine.seed(time(NULL));
	ready = true;
	return std::uniform_real_distribution<float>(min, max)(random_engine);
}

static void generate_section(struct Section *sect)
{
	sect->y_table_and_vertexdata_ready = false;
	int i;

	// wide and deep/tall
	for (i = 0; i < sect->mountains.size()/20; i++) {
		float h = 5*tanf(uniform_random_float(-1.4f, 1.4f));
		float w = uniform_random_float(fabsf(h), 3*fabsf(h));
		sect->mountains[i] = GaussianCurveMountain(w, h, uniform_random_float(0, SECTION_SIZE), uniform_random_float(0, SECTION_SIZE));
	}

	// narrow and shallow
	for (; i < sect->mountains.size(); i++) {
		float h = uniform_random_float(0.25f, 1.5f);
		float w = uniform_random_float(2*h, 5*h);
		if (rand() % 2)
			h = -h;
		sect->mountains[i] = GaussianCurveMountain(w, h, uniform_random_float(0, SECTION_SIZE), uniform_random_float(0, SECTION_SIZE));
	}

	// y=e^(-x^2) seems to be pretty much zero for |x| >= 3.
	// We use this to keep gaussian curves within the neighboring sections.
	int xzmin = -SECTION_SIZE, xzmax = 2*SECTION_SIZE;

	for (i = 0; i < sect->mountains.size(); i++) {
		float mindist = min4(
			sect->mountains[i].centerx - xzmin,
			sect->mountains[i].centerz - xzmin,
			xzmax - sect->mountains[i].centerx,
			xzmax - sect->mountains[i].centerz);
		sect->mountains[i].xzscale = min(sect->mountains[i].xzscale, mindist/3);
	}

	// This loop is too slow to run within a single frame
	for (int xidx = 0; xidx <= 3*SECTION_SIZE; xidx++) {
		for (int zidx = 0; zidx <= 3*SECTION_SIZE; zidx++) {
			int x = xidx - SECTION_SIZE;
			int z = zidx - SECTION_SIZE;

			float y = 0;
			for (int i = 0; i < sect->mountains.size(); i++) {
				float dx = x - sect->mountains[i].centerx;
				float dz = z - sect->mountains[i].centerz;
				float xzscale = sect->mountains[i].xzscale;
				y += sect->mountains[i].yscale * expf(-1/(xzscale*xzscale) * (dx*dx + dz*dz));
			}
			sect->raw_y_table[xidx][zidx] = y;
		}
	}
}

/*
You typically need many new sections at once, because neighbor sections affect the section
that needs to be added. There's a separate thread that generates them in the
background. After generating, a section can be added anywhere on the map.
*/
struct SectionQueue {
	// TODO: sects should be a vector
	struct Section sects[30];  // should have about 4x the room it typically needs, because corner cases
	int len;
	SDL_mutex *lock;  // hold this while adding/removing/checking sects or len
	bool quit;
};

static int section_preparing_thread(void *queueptr)
{
	SDL_SetThreadPriority(SDL_THREAD_PRIORITY_LOW);
	SectionQueue *queue = (SectionQueue *)queueptr;

	Section *tmp = (Section*)malloc(sizeof(*tmp));
	SDL_assert(tmp);

	while (!queue->quit) {
		int ret = SDL_LockMutex(queue->lock);
		SDL_assert(ret == 0);
		int len = queue->len;
		ret = SDL_UnlockMutex(queue->lock);
		SDL_assert(ret == 0);

		int maxlen = sizeof(queue->sects)/sizeof(queue->sects[0]);
		if (len == maxlen) {
			SDL_Delay(10);
			continue;
		}

		log_printf("There is room in section queue (%d/%d), generating a new section", len, maxlen);
		generate_section(tmp);  // slow

		ret = SDL_LockMutex(queue->lock);
		SDL_assert(ret == 0);
		queue->sects[queue->len++] = *tmp;
		ret = SDL_UnlockMutex(queue->lock);
		SDL_assert(ret == 0);
	}

	free(tmp);
	return 1234;  // ignored
}

struct Map {
	struct Section *sections;
	int nsections;
	int *itable;  // Hash table of indexes into sections array
	unsigned sectsalloced;  // space allocated in itable and sections

	struct SectionQueue queue;
	SDL_Thread *prepthread;

	GLuint shaderprogram;
	GLuint vbo;  // Vertex Buffer Object, represents triangles going to gpu
};

static unsigned section_hash(int startx, int startz)
{
	unsigned x = (unsigned)(startx / SECTION_SIZE);
	unsigned z = (unsigned)(startz / SECTION_SIZE);

	// both magic numbers are primes, to prevent patterns that place many numbers similarly
	return (x*7907u) ^ (z*4391u);
}

// For checking whether hash function is working well
#if 0
static void print_collision_percentage(const struct Map *map)
{
	int colls = 0, taken = 0;
	for (unsigned h = 0; h < map->sectsalloced; h++) {
		if (map->itable[h] == -1)
			continue;
		taken++;
		if (section_hash(map->sections[map->itable[h]].startx, map->sections[map->itable[h]].startz) % map->sectsalloced != h)
			colls++;
	}
	if (taken != 0)
		log_printf("itable: %d%% full, %d%% of filled slots are collisions",
			taken*100/map->sectsalloced, colls*100/taken);
}
#endif

static struct Section *find_section(const struct Map *map, int startx, int startz)
{
	SDL_assert(startx % SECTION_SIZE == 0);
	SDL_assert(startz % SECTION_SIZE == 0);

	if (map->sectsalloced == 0)
		return NULL;

	unsigned h = section_hash(startx, startz) % map->sectsalloced;
	while (map->itable[h] != -1) {
		struct Section *sec = &map->sections[map->itable[h]];
		if (sec->startx == startx && sec->startz == startz)
			return sec;
		h = (h+1) % map->sectsalloced;
	}
	return NULL;
}

static void add_section_to_itable(struct Map *map, int sectidx)
{
	int startx = map->sections[sectidx].startx;
	int startz = map->sections[sectidx].startz;

	unsigned h = section_hash(startx, startz) % map->sectsalloced;
	while (map->itable[h] != -1)
		h = (h+1) % map->sectsalloced;
	map->itable[h] = sectidx;
}

static void make_room_for_more_sections(struct Map *map, unsigned howmanymore)
{
	// +1 to ensure there's empty slot in itable, so "while (itable[h] != -1)" loops will terminate
	unsigned goal = map->nsections + howmanymore + 1;

	// allocate more than necessary, to prevent collisions in hash table
	goal = (unsigned)(1.4f * goal);  // TODO: choose a good magic number?

	unsigned oldalloc = map->sectsalloced;
	if (map->sectsalloced == 0)
		map->sectsalloced = 1;
	while (map->sectsalloced < goal)
		map->sectsalloced *= 2;
	if (map->sectsalloced == oldalloc)
		return;
	log_printf("growing section arrays: %u --> %u", oldalloc, map->sectsalloced);

	map->itable = (int*)realloc(map->itable, sizeof(map->itable[0])*map->sectsalloced);
	map->sections = (Section*)realloc(map->sections, sizeof(map->sections[0])*map->sectsalloced);
	SDL_assert(map->itable && map->sections);

	for (int h = 0; h < map->sectsalloced; h++)
		map->itable[h] = -1;
	for (int i = 0; i < map->nsections; i++)
		add_section_to_itable(map, i);
}

static struct Section *find_or_add_section(struct Map *map, int startx, int startz)
{
	struct Section *res = find_section(map, startx, startz);
	if (!res) {
		res = &map->sections[map->nsections];

		// Wait until there is an item in the queue
		while(1) {
			int ret = SDL_LockMutex(map->queue.lock);
			SDL_assert(ret == 0);

			bool empty = (map->queue.len == 0);
			if (!empty)
				*res = map->queue.sects[--map->queue.len];

			ret = SDL_UnlockMutex(map->queue.lock);
			SDL_assert(ret == 0);

			if (empty)
				SDL_Delay(1);
			else
				break;
		}

		res->startx = startx;
		res->startz = startz;
		add_section_to_itable(map, map->nsections);
		map->nsections++;
		log_printf("added a section, map now has %d sections", map->nsections);
	}
	return res;
}

static void ensure_y_table_is_ready(struct Map *map, struct Section *sect)
{
	if (sect->y_table_and_vertexdata_ready)
		return;

	struct Section *sections[9] = {
		find_or_add_section(map, sect->startx - SECTION_SIZE, sect->startz - SECTION_SIZE),
		find_or_add_section(map, sect->startx - SECTION_SIZE, sect->startz),
		find_or_add_section(map, sect->startx - SECTION_SIZE, sect->startz + SECTION_SIZE),
		find_or_add_section(map, sect->startx, sect->startz - SECTION_SIZE),
		sect,
		find_or_add_section(map, sect->startx, sect->startz + SECTION_SIZE),
		find_or_add_section(map, sect->startx + SECTION_SIZE, sect->startz - SECTION_SIZE),
		find_or_add_section(map, sect->startx + SECTION_SIZE, sect->startz),
		find_or_add_section(map, sect->startx + SECTION_SIZE, sect->startz + SECTION_SIZE),
	};

	for (int xidx = 0; xidx <= SECTION_SIZE; xidx++) {
		for (int zidx = 0; zidx <= SECTION_SIZE; zidx++) {
			float y = 0;
			for (struct Section **s = sections; s < sections+9; s++) {
				int xdiff = sect->startx - (*s)->startx;
				int zdiff = sect->startz - (*s)->startz;
				int ix = xidx + SECTION_SIZE + xdiff;
				int iz = zidx + SECTION_SIZE + zdiff;
				y += (*s)->raw_y_table[ix][iz];
			}

			sect->y_table[xidx][zidx] = y;
		}
	}

	int i = 0;
	for (int ix = 0; ix < SECTION_SIZE; ix++) {
		for (int iz = 0; iz < SECTION_SIZE; iz++) {
			float sx = sect->startx, sz = sect->startz;  // c++ sucks ass

			sect->vertexdata[i++] = std::array<vec3, 3>{
				vec3{sx + ix  , sect->y_table[ix  ][iz  ], sz + iz  },
				vec3{sx + ix+1, sect->y_table[ix+1][iz  ], sz + iz  },
				vec3{sx + ix  , sect->y_table[ix  ][iz+1], sz + iz+1},
			};
			sect->vertexdata[i++] = std::array<vec3, 3>{
				sx + ix+1, sect->y_table[ix+1][iz+1], sz + iz+1,
				sx + ix+1, sect->y_table[ix+1][iz  ], sz + iz  ,
				sx + ix  , sect->y_table[ix  ][iz+1], sz + iz+1,
			};
		}
	}
	SDL_assert(i == sect->vertexdata.size());

	sect->y_table_and_vertexdata_ready = true;
}

// round down to multiple of SECTION_SIZE
static int get_section_start_coordinate(float val)
{
	return (int)floorf(val / SECTION_SIZE) * SECTION_SIZE;
}

float map_getheight(struct Map *map, float x, float z)
{
	// Allocate many sections beforehand, doing it later can mess up pointers into sections array
	// One section is not enough, need to include its 8 neighbors too
	make_room_for_more_sections(map, 9);

	struct Section *sect = find_or_add_section(map, get_section_start_coordinate(x), get_section_start_coordinate(z));
	ensure_y_table_is_ready(map, sect);

	float ixfloat = x - sect->startx;
	float izfloat = z - sect->startz;
	int ix = (int)floorf(ixfloat);
	int iz = (int)floorf(izfloat);
	// TODO: some kind of modulo function?
	float t = ixfloat - ix;
	float u = izfloat - iz;

	// weighted average, weight describes how close to a given corner
	return (1-t)*(1-u)*sect->y_table[ix][iz]
		+ (1-t)*u*sect->y_table[ix][iz+1]
		+ t*(1-u)*sect->y_table[ix+1][iz]
		+ t*u*sect->y_table[ix+1][iz+1];
}

mat3 map_get_rotation(struct Map *map, float x, float z)
{
	// a bit of a hack, but works well enough
	float h = 0.01f;
	vec3 v = { 2*h, map_getheight(map,x+h,z) - map_getheight(map,x-h,z), 0 };
	vec3 w = { 0, map_getheight(map,x,z+h) - map_getheight(map,x,z-h), 2*h };

	// Ensure that v and w are perpendicular and length 1
	// https://en.wikipedia.org/wiki/Gram%E2%80%93Schmidt_process
	vec3_mul_float_inplace(&v, 1/sqrtf(vec3_dot(v,v)));
	vec3_sub_inplace(&w, vec3_mul_float(v, vec3_dot(v,w)));
	vec3_mul_float_inplace(&w, 1/sqrtf(vec3_dot(w,w)));

	vec3 cross = vec3_cross(w, v);
	mat3 res = {
		v.x, cross.x, w.x,
		v.y, cross.y, w.y,
		v.z, cross.z, w.z,
	};
	SDL_assert(fabsf(mat3_det(res) - 1) < 0.01f);
	return res;
}

void map_render(struct Map *map, const struct Camera *cam)
{
	glUseProgram(map->shaderprogram);
	glUniform3f(
		glGetUniformLocation(map->shaderprogram, "cameraLocation"),
		cam->location.x, cam->location.y, cam->location.z);
	glUniformMatrix3fv(
		glGetUniformLocation(map->shaderprogram, "world2cam"),
		1, true, &cam->world2cam.rows[0][0]);

	// If you change this, also change the shader in opengl_boilerplate.c
	int r = 80;

	int startxmin = get_section_start_coordinate(cam->location.x - r);
	int startxmax = get_section_start_coordinate(cam->location.x + r);
	int startzmin = get_section_start_coordinate(cam->location.z - r);
	int startzmax = get_section_start_coordinate(cam->location.z + r);

	// +1 because both ends inlusive
	int nx = (startxmax - startxmin)/SECTION_SIZE + 1;
	int nz = (startzmax - startzmin)/SECTION_SIZE + 1;
	int nsections = nx*nz;

	int maxsections = ((2*r)/SECTION_SIZE + 2)*((2*r)/SECTION_SIZE + 2);
	SDL_assert(nsections <= maxsections);

	// need +2 because one extra section in each direction
	make_room_for_more_sections(map, (nx+2)*(nz+2));

	if (map->vbo == 0) {
		glGenBuffers(1, &map->vbo);
		SDL_assert(map->vbo != 0);
		glBindBuffer(GL_ARRAY_BUFFER, map->vbo);
		glBufferData(GL_ARRAY_BUFFER, maxsections*sizeof(((Section*)NULL)->vertexdata), NULL, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	glBindBuffer(GL_ARRAY_BUFFER, map->vbo);
	int i = 0;
	for (int startx = startxmin; startx <= startxmax; startx += SECTION_SIZE) {
		for (int startz = startzmin; startz <= startzmax; startz += SECTION_SIZE) {
			// TODO: don't send all vertexdata to gpu, if same section still visible as last time?
			struct Section *sect = find_or_add_section(map, startx, startz);
			ensure_y_table_is_ready(map, sect);
			glBufferSubData(GL_ARRAY_BUFFER, i++*sizeof(sect->vertexdata), sizeof(sect->vertexdata), sect->vertexdata.data());
		}
	}
	SDL_assert(i == nsections);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glDrawArrays(GL_TRIANGLES, 0, nsections*TRIANGLES_PER_SECTION*3);

	glDisableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glUseProgram(0);
}

struct Map *map_new(void)
{
	Map *map = (Map*)calloc(1, sizeof(*map));
	SDL_assert(map);
	*map = Map{};

	map->queue.lock = SDL_CreateMutex();
	SDL_assert(map->queue.lock);

	map->prepthread = SDL_CreateThread(section_preparing_thread, "NameOfTheMapSectionGeneratorThread", &map->queue);
	SDL_assert(map->prepthread);

	const char *vertex_shader =
		"#version 330\n"
		"\n"
		"layout(location = 0) in vec3 position;\n"
		"uniform vec3 cameraLocation;\n"
		"uniform mat3 world2cam;\n"
		"smooth out vec4 vertexToFragmentColor;\n"
		"\n"
		"BOILERPLATE_GOES_HERE\n"
		"\n"
		"void main(void)\n"
		"{\n"
		"    vec3 pos = world2cam*(position - cameraLocation);\n"
		"    gl_Position = locationFromCameraToGlPosition(pos);\n"
		"\n"
		"    vec3 rgb = vec3(\n"
		"        pow(0.5 + atan((position.y + 5)/10)/3.1415, 2),\n"
		"        0.5*(0.5 + atan(position.y/10)/3.1415),\n"
		"        0.5 - atan(position.y/10)/3.1415\n"
		"    );\n"
		"    vertexToFragmentColor = darkerAtDistance(rgb, pos);\n"
		"}\n"
		;
	map->shaderprogram = opengl_boilerplate_create_shader_program(vertex_shader, NULL);

	return map;
}

// TODO: delete some of the opengl stuff?
void map_destroy(struct Map *map)
{
	map->queue.quit = true;
	SDL_WaitThread(map->prepthread, NULL);
	free(map->itable);
	free(map->sections);
	free(map);
}
