#include "map.h"
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <math.h>
#include "camera.h"
#include "log.h"

#define min(x, y) ((x)<(y) ? (x) : (y))
#define min4(a,b,c,d) min(min(a,b),min(c,d))

#define SECTION_SIZE 40  // side length of section square on xz plane
#define TRIANGLES_PER_SECTION (2*SECTION_SIZE*SECTION_SIZE)

struct Section {
	int startx, startz;

	/*
	y = yscale*e^(-(((x - centerx) / xzscale)^2 + ((z - centerz) / xzscale)^2))
	yscale can be negative, xzscale can't, center must be within map
	center coords are relative to section start, so that sections are easy to move
	*/
	struct GaussianCurveMountain { float xzscale,yscale,centerx,centerz; } mountains[100];

	/*
	ytable contains cached values for height of map, depending also on neighbor sections.

	Raw version:
		- contains enough values to cover neighbors too
		- does not take in account neighbors
		- is slow to compute
		- is always ready to be used, even when ytableready is false

	vertexdata is passed to the gpu for rendering, and represents triangles.
	Don't use it (or ytable) until ytableready is true.
	*/
	float ytableraw[3*SECTION_SIZE + 1][3*SECTION_SIZE + 1];
	float ytable[SECTION_SIZE + 1][SECTION_SIZE + 1];
	GLfloat vertexdata[TRIANGLES_PER_SECTION][9];
	bool ytableready;
};

static float uniform_random_float(float min, float max)
{
	return min + rand()*(max-min)/(float)RAND_MAX;
}

static void generate_section(struct Section *sect)
{
	sect->ytableready = false;

	int i;
	int n = sizeof(sect->mountains)/sizeof(sect->mountains[0]);

	// wide and deep/tall
	for (i = 0; i < n/20; i++) {
		float h = 5*tanf(uniform_random_float(-1.4f, 1.4f));
		float w = uniform_random_float(fabsf(h), 3*fabsf(h));
		sect->mountains[i] = (struct GaussianCurveMountain){
			.xzscale = w,
			.yscale = h,
			.centerx = uniform_random_float(0, SECTION_SIZE),
			.centerz = uniform_random_float(0, SECTION_SIZE),
		};
	}

	// narrow and shallow
	for (; i < n; i++) {
		float h = uniform_random_float(0.25f, 1.5f);
		float w = uniform_random_float(2*h, 5*h);
		if (rand() % 2)
			h = -h;
		sect->mountains[i] = (struct GaussianCurveMountain){
			.xzscale = w,
			.yscale = h,
			.centerx = uniform_random_float(0, SECTION_SIZE),
			.centerz = uniform_random_float(0, SECTION_SIZE),
		};
	}

	// y=e^(-x^2) seems to be pretty much zero for |x| >= 3.
	// We use this to keep gaussian curves within the neighboring sections.
	int xzmin = -SECTION_SIZE, xzmax = 2*SECTION_SIZE;

	for (i = 0; i < n; i++) {
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
			for (int i = 0; i < sizeof(sect->mountains) / sizeof(sect->mountains[0]); i++) {
				float dx = x - sect->mountains[i].centerx;
				float dz = z - sect->mountains[i].centerz;
				float xzscale = sect->mountains[i].xzscale;
				y += sect->mountains[i].yscale * expf(-1/(xzscale*xzscale) * (dx*dx + dz*dz));
			}
			sect->ytableraw[xidx][zidx] = y;
		}
	}
}

/*
You typically need many new sections at once, because neighbor sections affect the section
that needs to be added. There's a separate thread that generates them in the
background. After generating, a section can be added anywhere on the map.
*/
struct SectionQueue {
	struct Section sects[20];  // should have about 4x the room it typically needs, because corner cases
	int len;
	SDL_mutex *lock;  // hold this while adding/removing/checking sects or len
	bool quit;
};

static int section_preparing_thread(void *queueptr)
{
	SDL_SetThreadPriority(SDL_THREAD_PRIORITY_LOW);
	struct SectionQueue *queue = queueptr;

	struct Section *tmp = malloc(sizeof(*tmp));
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

	GLuint vbo;  // Vertex Buffer Object
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

	map->itable = realloc(map->itable, sizeof(map->itable[0])*map->sectsalloced);
	map->sections = realloc(map->sections, sizeof(map->sections[0])*map->sectsalloced);
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

static void ensure_ytable_is_ready(struct Map *map, struct Section *sect)
{
	if (sect->ytableready)
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
				y += (*s)->ytableraw[ix][iz];
			}

			sect->ytable[xidx][zidx] = y;
		}
	}

	float (*ptr)[9] = sect->vertexdata;
	for (int ix = 0; ix < SECTION_SIZE; ix++) {
		for (int iz = 0; iz < SECTION_SIZE; iz++) {
			float triangle1[] = {
				sect->startx + ix  , sect->ytable[ix  ][iz  ], sect->startz + iz  ,
				sect->startx + ix+1, sect->ytable[ix+1][iz  ], sect->startz + iz  ,
				sect->startx + ix  , sect->ytable[ix  ][iz+1], sect->startz + iz+1,
			};
			float triangle2[] = {
				sect->startx + ix+1, sect->ytable[ix+1][iz+1], sect->startz + iz+1,
				sect->startx + ix+1, sect->ytable[ix+1][iz  ], sect->startz + iz  ,
				sect->startx + ix  , sect->ytable[ix  ][iz+1], sect->startz + iz+1,
			};
			memcpy(*ptr++, triangle1, sizeof triangle1);
			memcpy(*ptr++, triangle2, sizeof triangle2);
		}
	}
	SDL_assert(ptr == sect->vertexdata + sizeof(sect->vertexdata)/sizeof(sect->vertexdata[0]));

	sect->ytableready = true;
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
	ensure_ytable_is_ready(map, sect);

	float ixfloat = x - sect->startx;
	float izfloat = z - sect->startz;
	int ix = (int)floorf(ixfloat);
	int iz = (int)floorf(izfloat);
	// TODO: some kind of modulo function?
	float t = ixfloat - ix;
	float u = izfloat - iz;

	// weighted average, weight describes how close to a given corner
	return (1-t)*(1-u)*sect->ytable[ix][iz]
		+ (1-t)*u*sect->ytable[ix][iz+1]
		+ t*(1-u)*sect->ytable[ix+1][iz]
		+ t*u*sect->ytable[ix+1][iz+1];
}

// TODO: rename this function
void map_drawgrid(struct Map *map, const struct Camera *cam)
{
	float r = 50;

	int startxmin = get_section_start_coordinate(cam->location.x - r);
	int startxmax = get_section_start_coordinate(cam->location.x + r);
	int startzmin = get_section_start_coordinate(cam->location.z - r);
	int startzmax = get_section_start_coordinate(cam->location.z + r);

	// +1 because both ends inlusive
	int nx = (startxmax - startxmin)/SECTION_SIZE + 1;
	int nz = (startzmax - startzmin)/SECTION_SIZE + 1;
	int nsections = nx*nz;

	int maxsections = ((int)(2*r/SECTION_SIZE) + 2)*((int)(2*r/SECTION_SIZE) + 2);
	SDL_assert(nsections <= maxsections);

	// need +2 because one extra section in each direction
	make_room_for_more_sections(map, (nx+2)*(nz+2));

	if (map->vbo == 0) {
		glGenBuffers(1, &map->vbo);
		SDL_assert(map->vbo != 0);
		glBindBuffer(GL_ARRAY_BUFFER, map->vbo);
		glBufferData(GL_ARRAY_BUFFER, maxsections*sizeof(((struct Section*)NULL)->vertexdata), NULL, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		log_printf("map vbo initialized");
	}

	glBindBuffer(GL_ARRAY_BUFFER, map->vbo);
	int i = 0;
	for (int startx = startxmin; startx <= startxmax; startx += SECTION_SIZE) {
		for (int startz = startzmin; startz <= startzmax; startz += SECTION_SIZE) {
			struct Section *sect = find_or_add_section(map, startx, startz);
			ensure_ytable_is_ready(map, sect);
			glBufferSubData(GL_ARRAY_BUFFER, i++*sizeof(sect->vertexdata), sizeof(sect->vertexdata), sect->vertexdata);
		}
	}
	SDL_assert(i == nsections);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glDrawArrays(GL_TRIANGLES, 0, nsections*TRIANGLES_PER_SECTION);

	glDisableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

struct Map *map_new(void)
{
	struct Map *map = malloc(sizeof(*map));
	SDL_assert(map);
	memset(map, 0, sizeof *map);

	map->queue.lock = SDL_CreateMutex();
	SDL_assert(map->queue.lock);

	map->prepthread = SDL_CreateThread(section_preparing_thread, "NameOfTheMapSectionGeneratorThread", &map->queue);
	SDL_assert(map->prepthread);

	return map;
}

void map_destroy(struct Map *map)
{
	map->queue.quit = true;
	SDL_WaitThread(map->prepthread, NULL);
	free(map->itable);
	free(map->sections);
	free(map);
}
