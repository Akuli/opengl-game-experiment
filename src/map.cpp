#include "map.hpp"
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>
#include "camera.hpp"
#include "log.hpp"
#include "opengl_boilerplate.hpp"

static constexpr int SECTION_SIZE = 40;  // side length of section square on xz plane
static constexpr int TRIANGLES_PER_SECTION = 2*SECTION_SIZE*SECTION_SIZE;

struct GaussianCurveMountain {
	float xzscale, yscale, centerx, centerz;
	GaussianCurveMountain() = default;
	GaussianCurveMountain(float xzscale, float yscale, float centerx, float centerz) : xzscale(xzscale),yscale(yscale),centerx(centerx),centerz(centerz) {}
};

struct Section {
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

	Section() = default;
	Section(const Section&) = delete;
};

static float uniform_random_float(float min, float max)
{
	// I tried writing this in "modern C++" style but that was more complicated
	return lerp(min, max, std::rand() / (float)RAND_MAX);
}

static void generate_section(Section& section)
{
	section.y_table_and_vertexdata_ready = false;
	int i;

	// wide and deep/tall
	for (i = 0; i < section.mountains.size()/20; i++) {
		float h = 5*std::tan(uniform_random_float(-1.4f, 1.4f));
		float w = uniform_random_float(std::abs(h), 3*std::abs(h));
		section.mountains[i] = GaussianCurveMountain(w, h, uniform_random_float(0, SECTION_SIZE), uniform_random_float(0, SECTION_SIZE));
	}

	// narrow and shallow
	for (; i < section.mountains.size(); i++) {
		float h = uniform_random_float(0.25f, 1.5f);
		float w = uniform_random_float(2*h, 5*h);
		if (rand() % 2)
			h = -h;
		section.mountains[i] = GaussianCurveMountain(w, h, uniform_random_float(0, SECTION_SIZE), uniform_random_float(0, SECTION_SIZE));
	}

	// y=e^(-x^2) seems to be pretty much zero for |x| >= 3.
	// We use this to keep gaussian curves within the neighboring sections.
	int xzmin = -SECTION_SIZE, xzmax = 2*SECTION_SIZE;

	for (i = 0; i < section.mountains.size(); i++) {
		float mindist = std::min({
			section.mountains[i].centerx - xzmin,
			section.mountains[i].centerz - xzmin,
			xzmax - section.mountains[i].centerx,
			xzmax - section.mountains[i].centerz,
		});
		section.mountains[i].xzscale = std::min(section.mountains[i].xzscale, mindist/3);
	}

	// This loop is too slow to run within a single frame
	for (int xidx = 0; xidx <= 3*SECTION_SIZE; xidx++) {
		for (int zidx = 0; zidx <= 3*SECTION_SIZE; zidx++) {
			int x = xidx - SECTION_SIZE;
			int z = zidx - SECTION_SIZE;

			float y = 0;
			for (int i = 0; i < section.mountains.size(); i++) {
				float dx = x - section.mountains[i].centerx;
				float dz = z - section.mountains[i].centerz;
				float xzscale = section.mountains[i].xzscale;
				y += section.mountains[i].yscale * expf(-1/(xzscale*xzscale) * (dx*dx + dz*dz));
			}
			section.raw_y_table[xidx][zidx] = y;
		}
	}
}

/*
You typically need many new sections at once, because neighbor sections affect the section
that needs to be added. There's a separate thread that generates them in the
background. After generating, a section can be added anywhere on the map.
*/
struct SectionQueue {
	std::vector<std::unique_ptr<Section>> sections;
	SDL_mutex *lock;  // hold this while adding/removing/checking sects or len
	bool quit;
};

static int section_preparing_thread(void *queueptr)
{
	SDL_SetThreadPriority(SDL_THREAD_PRIORITY_LOW);
	SectionQueue *queue = (SectionQueue *)queueptr;

	while (!queue->quit) {
		int ret = SDL_LockMutex(queue->lock);
		SDL_assert(ret == 0);
		int len = queue->sections.size();
		ret = SDL_UnlockMutex(queue->lock);
		SDL_assert(ret == 0);

		int maxlen = 30;  // about 4x usual size, in case corner cases do something weird
		if (len == maxlen) {
			SDL_Delay(10);
			continue;
		}

		log_printf("There is room in section queue (%d/%d), generating a new section", len, maxlen);
		std::unique_ptr<Section> tmp = std::make_unique<Section>();
		generate_section(*tmp);  // slow

		ret = SDL_LockMutex(queue->lock);
		SDL_assert(ret == 0);
		queue->sections.push_back(std::move(tmp));
		ret = SDL_UnlockMutex(queue->lock);
		SDL_assert(ret == 0);
	}

	return 1234;  // ignored
}

// pairs aren't hashable :(
// https://stackoverflow.com/a/32685618
struct IntPairHasher {
	size_t operator() (const std::pair<int,int> &pair) const {
		size_t h1 = std::hash<int>{}(pair.first);
		size_t h2 = std::hash<int>{}(pair.second);
		// both magic numbers are primes, to prevent patterns that place many numbers similarly
		return (h1*7907u) ^ (h2*4391u);
	}
};

struct MapPrivate {
	std::unordered_map<std::pair<int, int>, std::unique_ptr<Section>, IntPairHasher> sections;

	SectionQueue queue;
	SDL_Thread *prepthread;

	GLuint shaderprogram;
	GLuint vbo;  // Vertex Buffer Object, represents triangles going to gpu
};

static Section *find_or_add_section(MapPrivate& map, int startx, int startz)
{
	std::pair<int, int> key = { startx, startz };

	if (map.sections.find(std::make_pair(startx, startz)) == map.sections.end()) {
		// Wait until there is an item in the queue
		while(1) {
			int ret = SDL_LockMutex(map.queue.lock);
			SDL_assert(ret == 0);

			bool empty = map.queue.sections.empty();
			if (!empty) {
				map.sections[key] = std::move(map.queue.sections.end()[-1]);
				map.queue.sections.pop_back();
			}

			ret = SDL_UnlockMutex(map.queue.lock);
			SDL_assert(ret == 0);

			if (empty)
				SDL_Delay(1);
			else
				break;
		}

		log_printf("added a section, map now has %d sections", (int)map.sections.size());
	}

	return &*map.sections[key];
}

static void ensure_y_table_is_ready(MapPrivate& map, int startx, int startz)
{
	SDL_assert(map.sections.find(std::make_pair(startx, startz)) != map.sections.end());
	Section *section = map.sections[std::make_pair(startx, startz)].get();
	if (section->y_table_and_vertexdata_ready)
		return;

	for (int xidx = 0; xidx <= SECTION_SIZE; xidx++) {
		for (int zidx = 0; zidx <= SECTION_SIZE; zidx++) {
			float y = 0;
			for (int xdiff = -SECTION_SIZE; xdiff <= SECTION_SIZE; xdiff += SECTION_SIZE) {
				for (int zdiff = -SECTION_SIZE; zdiff <= SECTION_SIZE; zdiff += SECTION_SIZE) {
					Section *s = find_or_add_section(map, startx + xdiff, startz + zdiff);
					int ix = xidx + SECTION_SIZE - xdiff;
					int iz = zidx + SECTION_SIZE - zdiff;
					y += s->raw_y_table[ix][iz];
				}
			}
			section->y_table[xidx][zidx] = y;
		}
	}

	int i = 0;
	for (int ix = 0; ix < SECTION_SIZE; ix++) {
		for (int iz = 0; iz < SECTION_SIZE; iz++) {
			float sx = startx, sz = startz;  // c++ sucks ass
			section->vertexdata[i++] = std::array<vec3, 3>{
				vec3{sx + ix  , section->y_table[ix  ][iz  ], sz + iz  },
				vec3{sx + ix+1, section->y_table[ix+1][iz  ], sz + iz  },
				vec3{sx + ix  , section->y_table[ix  ][iz+1], sz + iz+1},
			};
			section->vertexdata[i++] = std::array<vec3, 3>{
				vec3{sx + ix+1, section->y_table[ix+1][iz+1], sz + iz+1},
				vec3{sx + ix+1, section->y_table[ix+1][iz  ], sz + iz  },
				vec3{sx + ix  , section->y_table[ix  ][iz+1], sz + iz+1},
			};
		}
	}
	SDL_assert(i == section->vertexdata.size());

	section->y_table_and_vertexdata_ready = true;
}

// round down to multiple of SECTION_SIZE
static int get_section_start_coordinate(float val)
{
	return (int)std::floor(val / SECTION_SIZE) * SECTION_SIZE;
}

float Map::get_height(float x, float z)
{
	int startx = get_section_start_coordinate(x), startz = get_section_start_coordinate(z);
	Section *section = find_or_add_section(*this->priv, startx, startz);
	ensure_y_table_is_ready(*this->priv, startx, startz);

	float ixfloat = x - startx;
	float izfloat = z - startz;
	int ix = (int)std::floor(ixfloat);
	int iz = (int)std::floor(izfloat);
	// TODO: some kind of modulo function?
	float t = ixfloat - ix;
	float u = izfloat - iz;

	// weighted average, weight describes how close to a given corner
	return (1-t)*(1-u)*section->y_table[ix][iz]
		+ (1-t)*u*section->y_table[ix][iz+1]
		+ t*(1-u)*section->y_table[ix+1][iz]
		+ t*u*section->y_table[ix+1][iz+1];
}

mat3 Map::get_rotation_matrix(float x, float z)
{
	// a bit of a hack, but works well enough
	float h = 0.01f;
	vec3 v = { 2*h, this->get_height(x+h,z) - this->get_height(x-h,z), 0 };
	vec3 w = { 0, this->get_height(x,z+h) - this->get_height(x,z-h), 2*h };

	// Ensure that v and w are perpendicular and length 1
	// https://en.wikipedia.org/wiki/Gram%E2%80%93Schmidt_process
	v /= std::sqrt(v.dot(v));
	w -= v*v.dot(w);
	w /= std::sqrt(w.dot(w));

	vec3 cross = w.cross(v);
	mat3 res = {
		v.x, cross.x, w.x,
		v.y, cross.y, w.y,
		v.z, cross.z, w.z,
	};
	SDL_assert(std::abs(res.det() - 1) < 0.01f);
	return res;
}

void Map::render(const Camera& cam)
{
	glUseProgram(this->priv->shaderprogram);
	glUniform3f(
		glGetUniformLocation(this->priv->shaderprogram, "cameraLocation"),
		cam.location.x, cam.location.y, cam.location.z);
	glUniformMatrix3fv(
		glGetUniformLocation(this->priv->shaderprogram, "world2cam"),
		1, true, &cam.world2cam.rows[0][0]);

	// If you change this, also change the shader in opengl_boilerplate.c
	int r = 80;

	int startxmin = get_section_start_coordinate(cam.location.x - r);
	int startxmax = get_section_start_coordinate(cam.location.x + r);
	int startzmin = get_section_start_coordinate(cam.location.z - r);
	int startzmax = get_section_start_coordinate(cam.location.z + r);

	// +1 because both ends inlusive
	int nx = (startxmax - startxmin)/SECTION_SIZE + 1;
	int nz = (startzmax - startzmin)/SECTION_SIZE + 1;
	int nsections = nx*nz;

	int maxsections = ((2*r)/SECTION_SIZE + 2)*((2*r)/SECTION_SIZE + 2);
	SDL_assert(nsections <= maxsections);

	if (this->priv->vbo == 0) {
		glGenBuffers(1, &this->priv->vbo);
		SDL_assert(this->priv->vbo != 0);
		glBindBuffer(GL_ARRAY_BUFFER, this->priv->vbo);
		glBufferData(GL_ARRAY_BUFFER, maxsections*sizeof(((Section*)nullptr)->vertexdata), nullptr, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	glBindBuffer(GL_ARRAY_BUFFER, this->priv->vbo);
	int i = 0;
	for (int startx = startxmin; startx <= startxmax; startx += SECTION_SIZE) {
		for (int startz = startzmin; startz <= startzmax; startz += SECTION_SIZE) {
			// TODO: don't send all vertexdata to gpu, if same section still visible as last time?
			Section *section = find_or_add_section(*this->priv, startx, startz);
			ensure_y_table_is_ready(*this->priv, startx, startz);
			glBufferSubData(GL_ARRAY_BUFFER, i++*sizeof(section->vertexdata), sizeof(section->vertexdata), section->vertexdata.data());
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

Map::Map()
{
	this->priv = std::make_unique<MapPrivate>();
	this->priv->queue.lock = SDL_CreateMutex();
	SDL_assert(this->priv->queue.lock);

	this->priv->prepthread = SDL_CreateThread(section_preparing_thread, "NameOfTheMapSectionGeneratorThread", &this->priv->queue);
	SDL_assert(this->priv->prepthread);

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
	this->priv->shaderprogram = OpenglBoilerplate::create_shader_program(vertex_shader);
}

Map::~Map()
{
	// TODO: delete some of the opengl stuff?
	this->priv->queue.quit = true;
	SDL_WaitThread(this->priv->prepthread, nullptr);
	SDL_DestroyMutex(this->priv->queue.lock);
}
