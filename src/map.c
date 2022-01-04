#include "map.h"
#include "camera.h"
#include "log.h"
#include <math.h>

#define min(x, y) ((x)<(y) ? (x) : (y))
#define min4(a,b,c,d) min(min(a,b),min(c,d))

static float uniform_random_float(float min, float max)
{
	return min + rand()*(max-min)/(float)RAND_MAX;
}

static void generate_section(struct Section *sect, int startx, int startz)
{
	SDL_assert(startx % SECTION_SIZE == 0);
	SDL_assert(startz % SECTION_SIZE == 0);
	sect->startx = startx;
	sect->startz = startz;
	sect->ytableready = false;

	int i;
	int n = sizeof(sect->mountains)/sizeof(sect->mountains[0]);

	// wide and deep/tall
	for (i = 0; i < n/20; i++) {
		float h = tanf(uniform_random_float(-1.4f, 1.4f));
		float w = uniform_random_float(fabsf(h), 3*fabsf(h));
		sect->mountains[i] = (struct GaussianCurveMountain){
			.xzscale = w,
			.yscale = h,
			.centerx = uniform_random_float(startx, startx + SECTION_SIZE),
			.centerz = uniform_random_float(startz, startz + SECTION_SIZE),
		};
	}

	// narrow and shallow
	for (; i < n; i++) {
		float h = uniform_random_float(0.05f, 0.3f);
		float w = uniform_random_float(2*h, 5*h);
		if (rand() % 2)
			h = -h;
		sect->mountains[i] = (struct GaussianCurveMountain){
			.xzscale = w,
			.yscale = h,
			.centerx = uniform_random_float(startx, startx + SECTION_SIZE),
			.centerz = uniform_random_float(startz, startz + SECTION_SIZE),
		};
	}

	// y=e^(-x^2) seems to be pretty much zero for |x| >= 2.5.
	// We use this to keep gaussian curves within the neighboring sections.
	int xmin = startx - SECTION_SIZE;
	int zmin = startz - SECTION_SIZE;
	int xmax = startx + 2*SECTION_SIZE;
	int zmax = startz + 2*SECTION_SIZE;

	for (i = 0; i < n; i++) {
		float mindist = min4(
			sect->mountains[i].centerx - xmin,
			sect->mountains[i].centerz - zmin,
			xmax - sect->mountains[i].centerx,
			zmax - sect->mountains[i].centerz);
		sect->mountains[i].xzscale = min(sect->mountains[i].xzscale, mindist/2.5f);
	}
}

static unsigned section_hash(int startx, int startz)
{
	return (unsigned)(startx / SECTION_SIZE * 17) ^ (unsigned)(startz / SECTION_SIZE * 29);
}

static struct Section *find_section(const struct Map *map, int startx, int startz)
{
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

static void grow_section_arrays(struct Map *map)
{
	unsigned oldalloc = map->sectsalloced;
	map->sectsalloced *= 2;
	if (map->sectsalloced == 0)
		map->sectsalloced = 16;  // chosen so that a few grows occur during startup (exposes bugs)
	log_printf("growing section arrays: %u --> %u", oldalloc, map->sectsalloced);

	map->itable = realloc(map->itable, sizeof(map->itable[0])*map->sectsalloced);
	map->sections = realloc(map->sections, sizeof(map->sections[0])*map->sectsalloced);
	SDL_assert(map->itable && map->sections);

	for (int h = 0; h < map->sectsalloced; h++)
		map->itable[h] = -1;

	for (int i = 0; i < map->nsections; i++) {
		unsigned h = section_hash(map->sections[i].startx, map->sections[i].startz) % map->sectsalloced;
		while (map->itable[h] != -1)
			h = (h+1) % map->sectsalloced;
		map->itable[h] = i;
	}

}

static void add_section_to_itable(struct Map *map, int sectidx)
{
	int startx = map->sections[sectidx].startx;
	int startz = map->sections[sectidx].startz;
	unsigned h = section_hash(startx, startz) % map->sectsalloced;
	while (map->itable[h] != -1)
		h = (h+1) % map->sectsalloced;
	map->itable[h] = sectidx;

	// For checking whether hash function is working well
	/*
	char s[2000] = {0};
	SDL_assert(map->sectsalloced < sizeof s);
	for (unsigned h = 0; h < map->sectsalloced; h++)
		s[h] = map->itable[h]==-1 ? ' ' : 'x';
	log_printf("|%s|", s);
	*/
}

static struct Section *find_or_add_section(struct Map *map, int startx, int startz)
{
	struct Section *res = find_section(map, startx, startz);
	if (!res) {
		log_printf("there are %d sections, adding one more to startx=%d startz=%d",
			map->nsections, startx, startz);
		res = &map->sections[map->nsections];
		generate_section(res, startx, startz);
		add_section_to_itable(map, map->nsections);
		map->nsections++;
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

	for (int xidx = 0; xidx <= SECTION_SIZE*YTABLE_ITEMS_PER_UNIT; xidx++) {
		for (int zidx = 0; zidx <= SECTION_SIZE*YTABLE_ITEMS_PER_UNIT; zidx++) {
			float gap = 1.0f / YTABLE_ITEMS_PER_UNIT;
			float x = sect->startx + xidx*gap;
			float z = sect->startz + zidx*gap;

			float y = 0;
			for (struct Section **s = sections; s < sections+9; s++) {
				for (int i = 0; i < sizeof((*s)->mountains) / sizeof((*s)->mountains[0]); i++) {
					float dx = x - (*s)->mountains[i].centerx;
					float dz = z - (*s)->mountains[i].centerz;
					float xzscale = (*s)->mountains[i].xzscale;
					y += (*s)->mountains[i].yscale * expf(-1/(xzscale*xzscale) * (dx*dx + dz*dz));
				}
			}
			sect->ytable[xidx][zidx] = y;
		}
	}

	sect->ytableready = true;
}

// round down to multiple of SECTION_SIZE
static int get_section_start_coordinate(float val)
{
	return (int)floorf(val / SECTION_SIZE) * SECTION_SIZE;
}

static void make_room_for_more_sections(struct Map *map, int howmanymore)
{
	// +1 to ensure there's empty slot in itable, so "while (itable[h] != -1)" loops will terminate
	while (map->nsections + howmanymore + 1 > map->sectsalloced*0.7f)  // TODO: choose good magic number?
		grow_section_arrays(map);
}

float map_getheight(struct Map *map, float x, float z)
{
	// Allocate many sections beforehand, doing it later can mess up pointers into sections array
	// One section is not enough, need to include its 8 neighbors too
	make_room_for_more_sections(map, 9);

	struct Section *sect = find_or_add_section(map, get_section_start_coordinate(x), get_section_start_coordinate(z));
	ensure_ytable_is_ready(map, sect);

	float ixfloat = (x - sect->startx)*YTABLE_ITEMS_PER_UNIT;
	float izfloat = (z - sect->startz)*YTABLE_ITEMS_PER_UNIT;
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

#define RADIUS 10

void map_drawgrid(struct Map *map, const struct Camera *cam)
{
	int startxmin = get_section_start_coordinate(cam->location.x - RADIUS);
	int startxmax = get_section_start_coordinate(cam->location.x + RADIUS);
	int startzmin = get_section_start_coordinate(cam->location.z - RADIUS);
	int startzmax = get_section_start_coordinate(cam->location.z + RADIUS);

	// In x and z directions, need +2 (one extra in each direction) and +1 (both ends are inclusive)
	make_room_for_more_sections(map, (startxmax - startxmin + 3)*(startzmax - startzmin + 3));

	for (int startx = startxmin; startx <= startxmax; startx += SECTION_SIZE) {
		for (int startz = startzmin; startz <= startzmax; startz += SECTION_SIZE) {
			struct Section *sect = find_or_add_section(map, startx, startz);
			ensure_ytable_is_ready(map, sect);
			for (int ix = 0; ix < SECTION_SIZE*YTABLE_ITEMS_PER_UNIT; ix++) {
				for (int iz = 0; iz < SECTION_SIZE*YTABLE_ITEMS_PER_UNIT; iz++) {
					float gap = 1.0f / YTABLE_ITEMS_PER_UNIT;
					float x = sect->startx + gap*ix;
					float z = sect->startz + gap*iz;
					float dx = x - cam->location.x;
					float dz = z - cam->location.z;
					if (dx*dx + dz*dz < RADIUS*RADIUS && (dx+gap)*(dx+gap) + dz*dz < RADIUS*RADIUS)
						camera_drawline(cam, (Vec3){x,sect->ytable[ix][iz],z}, (Vec3){x+gap,sect->ytable[ix+1][iz],z});
					if (dx*dx + dz*dz < RADIUS*RADIUS && dx*dx + (dz+gap)*(dz+gap) < RADIUS*RADIUS)
						camera_drawline(cam, (Vec3){x,sect->ytable[ix][iz],z}, (Vec3){x,sect->ytable[ix][iz+1],z+gap});
				}
			}
		}
	}
}

void map_freebuffers(const struct Map *map)
{
	free(map->itable);
	free(map->sections);
}
