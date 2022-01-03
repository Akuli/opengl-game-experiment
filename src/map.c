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
	return (unsigned)(startx / SECTION_SIZE) ^ (unsigned)(startz / SECTION_SIZE);
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
}

static struct Section *find_or_add_section(struct Map *map, int startx, int startz)
{
	struct Section *res = find_section(map, startx, startz);
	if (!res) {
		// Make sure there's always 1 empty slot, makes searches easier
		if (map->nsections+2 >= map->sectsalloced*0.7f)  // TODO: choose good magic number
			grow_section_arrays(map);

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

	struct Section *sections[9];
	int i = 0;
	sections[i++] = sect;
	sections[i++] = find_or_add_section(map, sect->startx - SECTION_SIZE, sect->startz - SECTION_SIZE);
	sections[i++] = find_or_add_section(map, sect->startx - SECTION_SIZE, sect->startz);
	sections[i++] = find_or_add_section(map, sect->startx - SECTION_SIZE, sect->startz + SECTION_SIZE);
	sections[i++] = find_or_add_section(map, sect->startx, sect->startz - SECTION_SIZE);
	sections[i++] = find_or_add_section(map, sect->startx, sect->startz + SECTION_SIZE);
	sections[i++] = find_or_add_section(map, sect->startx + SECTION_SIZE, sect->startz - SECTION_SIZE);
	sections[i++] = find_or_add_section(map, sect->startx + SECTION_SIZE, sect->startz);
	sections[i++] = find_or_add_section(map, sect->startx + SECTION_SIZE, sect->startz + SECTION_SIZE);

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

float map_getheight(struct Map *map, float x, float z)
{
	// coords of middle section
	int mx = (int)floorf(x / SECTION_SIZE) * SECTION_SIZE;
	int mz = (int)floorf(z / SECTION_SIZE) * SECTION_SIZE;

	/*
	Allocate many sections now, doing it later can mess up pointers into sections array
	We need, at worst:
		1		new section
		+ 8		neighbor sections
		+ 1		empty slot in itable to ensure "while (itable[h] != -1)" loops will terminate
	*/
	while (map->nsections+1+8+1 >= map->sectsalloced*0.7f)  // TODO: choose good magic number?
		grow_section_arrays(map);

	struct Section *sect = find_or_add_section(map, mx, mz);
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

#define GRID_LINES_PER_UNIT 5
#define RADIUS 10

void map_drawgrid(struct Map *map, const struct Camera *cam)
{
	float spacing = 1.0f / GRID_LINES_PER_UNIT;

	static float heights[2*RADIUS*GRID_LINES_PER_UNIT + 1][2*RADIUS*GRID_LINES_PER_UNIT + 1];

	// Middle element of heights goes to this location
	float midx = roundf(cam->location.x / spacing) * spacing;
	float midz = roundf(cam->location.z / spacing) * spacing;

	for (int xidx = 0; xidx <= 2*RADIUS*GRID_LINES_PER_UNIT; xidx++) {
		for (int zidx = 0; zidx <= 2*RADIUS*GRID_LINES_PER_UNIT; zidx++) {
			float x = midx + (xidx - RADIUS*GRID_LINES_PER_UNIT)*spacing;
			float z = midz + (zidx - RADIUS*GRID_LINES_PER_UNIT)*spacing;
			float dx = x - cam->location.x;
			float dz = z - cam->location.z;
			if (dx*dx + dz*dz > RADIUS*RADIUS)
				heights[xidx][zidx] = NAN;
			else
				heights[xidx][zidx] = map_getheight(map, x, z);
		}
	}

	for (int xidx = 0; xidx < 2*RADIUS*GRID_LINES_PER_UNIT; xidx++) {
		for (int zidx = 0; zidx < 2*RADIUS*GRID_LINES_PER_UNIT; zidx++) {
			float y = heights[xidx][zidx];
			if (isnan(y))
				continue;
			float x = midx + (xidx - RADIUS*GRID_LINES_PER_UNIT)*spacing;
			float z = midz + (zidx - RADIUS*GRID_LINES_PER_UNIT)*spacing;

			if (!isnan(heights[xidx+1][zidx]))
				camera_drawline(cam, (Vec3){x,y,z}, (Vec3){x+spacing,heights[xidx+1][zidx],z});
			if (!isnan(heights[xidx][zidx+1]))
				camera_drawline(cam, (Vec3){x,y,z}, (Vec3){x,heights[xidx][zidx+1],z+spacing});
		}
	}
}
