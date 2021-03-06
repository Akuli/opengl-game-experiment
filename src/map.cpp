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
#include "config.hpp"
#include "entity.hpp"
#include "enemy.hpp"
#include "linalg.hpp"
#include "log.hpp"
#include "misc.hpp"
#include "opengl_boilerplate.hpp"

static constexpr int SECTION_SIZE = 40;  // side length of section square on xz plane
static constexpr int TRIANGLES_PER_SECTION = 2*SECTION_SIZE*SECTION_SIZE;

struct GaussianCurveMountain {
	float xzscale, yscale, centerx, centerz;
};

struct Section {
	Section() = default;
	Section(const Section&) = delete;

	std::vector<Enemy> enemies;

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

static void generate_section(Section& section)
{
	section.y_table_and_vertexdata_ready = false;
	int i;

	// wide and deep/tall
	for (i = 0; i < section.mountains.size()/20; i++) {
		float h = 5*std::tan(uniform_random_float(-1.4f, 1.4f));
		float w = uniform_random_float(std::abs(h), 3*std::abs(h));
		section.mountains[i] = GaussianCurveMountain{w, h, uniform_random_float(0, SECTION_SIZE), uniform_random_float(0, SECTION_SIZE)};
	}

	// narrow and shallow
	for (; i < section.mountains.size(); i++) {
		float h = uniform_random_float(0.25f, 1.5f);
		float w = uniform_random_float(2*h, 5*h);
		if (rand() % 2)
			h = -h;
		section.mountains[i] = GaussianCurveMountain{w, h, uniform_random_float(0, SECTION_SIZE), uniform_random_float(0, SECTION_SIZE)};
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

		//log_printf("There is room in section queue (%d/%d), generating a new section", len, maxlen);
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
	size_t operator()(const std::pair<int,int>& pair) const {
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
		int ret = SDL_LockMutex(map.queue.lock);
		SDL_assert(ret == 0);

		std::unique_ptr<Section> section = nullptr;
		if (!map.queue.sections.empty()) {
			section = std::move(map.queue.sections.end()[-1]);
			map.queue.sections.pop_back();
		}

		ret = SDL_UnlockMutex(map.queue.lock);
		SDL_assert(ret == 0);

		if (!section) {
			log_printf("Section queue was empty, generating a section outside queue");
			section = std::make_unique<Section>();
			generate_section(*section);  // slow
		}

		map.sections[key] = std::move(section);
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

vec3 Map::get_normal_vector(float x, float z)
{
	float h = 0.5f;  // Bigger value --> smoother but less accurate result
	vec3 v = { 2*h, this->get_height(x+h,z) - this->get_height(x-h,z), 0 };
	vec3 w = { 0, this->get_height(x,z+h) - this->get_height(x,z-h), 2*h };
	return w.cross(v);
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

	int startxmin = get_section_start_coordinate(cam.location.x - VIEW_RADIUS);
	int startxmax = get_section_start_coordinate(cam.location.x + VIEW_RADIUS);
	int startzmin = get_section_start_coordinate(cam.location.z - VIEW_RADIUS);
	int startzmax = get_section_start_coordinate(cam.location.z + VIEW_RADIUS);

	// +1 because both ends inlusive
	int nx = (startxmax - startxmin)/SECTION_SIZE + 1;
	int nz = (startzmax - startzmin)/SECTION_SIZE + 1;
	int nsections = nx*nz;

	int maxsections = ((2*VIEW_RADIUS)/SECTION_SIZE + 2)*((2*VIEW_RADIUS)/SECTION_SIZE + 2);
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


void Map::add_enemy(const Enemy& enemy) {
	int startx = get_section_start_coordinate(enemy.entity.location.x);
	int startz = get_section_start_coordinate(enemy.entity.location.z);
	Section *section = find_or_add_section(*this->priv, startx, startz);
	section->enemies.push_back(enemy);
}

int Map::get_number_of_enemies() const {
	int result = 0;
	for (const auto& item : this->priv->sections)
		result += item.second->enemies.size();
	return result;
}


static bool circle_and_line_segment_intersect(vec2 center, float r, vec2 start, vec2 end)
{
	// Find t so that the point lerp(start,end,t) is as close to the center as possible
	vec2 dir = end-start;
	float t = unlerp(start.dot(dir), end.dot(dir), center.dot(dir));

	// Make sure we stay on the line segment
	if (t < 0) t = 0;
	if (t > 1) t = 1;

	// Check if the point we got is in the circle
	return (center - lerp(start, end, t)).length_squared() < r*r;
}

static bool circle_intersects_section(vec2 center, float r, int section_start_x, int section_start_z)
{
	// If the entire circle is inside the section, this check is needed
	if (get_section_start_coordinate(center.x) == section_start_x &&
		get_section_start_coordinate(center.y) == section_start_z)
	{
		return true;
	}

	vec2 corners[4] = {
		vec2(section_start_x, section_start_z),
		vec2(section_start_x, section_start_z+SECTION_SIZE),
		vec2(section_start_x+SECTION_SIZE, section_start_z+SECTION_SIZE),
		vec2(section_start_x+SECTION_SIZE, section_start_z),
	};

	for (int i = 0; i < 4; i++) {
		vec2 start = corners[i];
		vec2 end = corners[(i+1)%4];

		if (circle_and_line_segment_intersect(center, r, start, end))
			return true;
	}
	return false;
}


struct LocationAndSection {
	int startx, startz;
	Section* section;
};

static std::vector<LocationAndSection> find_sections_within_circle(const MapPrivate& map, float center_x, float center_z, float radius) {
	std::vector<LocationAndSection> result = {};
	int startx_min = get_section_start_coordinate(center_x - radius);
	int startx_max = get_section_start_coordinate(center_x + radius);
	int startz_min = get_section_start_coordinate(center_z - radius);
	int startz_max = get_section_start_coordinate(center_z + radius);

	for (int startx = startx_min; startx <= startx_max; startx += SECTION_SIZE) {
		for (int startz = startz_min; startz <= startz_max; startz += SECTION_SIZE) {
			if (circle_intersects_section(vec2{center_x,center_z}, radius, startx, startz)) {
				auto find_result = map.sections.find(std::make_pair(startx, startz));
				if (find_result != map.sections.end())
					result.push_back({ startx, startz, find_result->second.get() });
			}
		}
	}
	return result;
}

std::vector<const Enemy*> Map::find_enemies_within_circle(float center_x, float center_z, float radius) const
{
	std::vector<const Enemy*> result = {};
	for (LocationAndSection las : find_sections_within_circle(*this->priv, center_x, center_z, radius)) {
		for (int i = 0; i < las.section->enemies.size(); i++) {
			Enemy* enemy = &las.section->enemies.data()[i];
			float dx = center_x - enemy->entity.location.x;
			float dz = center_z - enemy->entity.location.z;
			if (dx*dx + dz*dz < radius*radius)
				result.push_back(&las.section->enemies.data()[i]);
		}
	}
	return result;
}

std::vector<const Enemy*> Map::find_colliding_enemies(const Entity& collide_with)
{
	std::vector<const Enemy*> result = {};

	// TODO: hard-coded 10 also appears in a few other places
	for (const Enemy* enemy : this->find_enemies_within_circle(collide_with.location.x, collide_with.location.z, 10)) {
		if (enemy->entity.collides_with(collide_with, *this))
			result.push_back(enemy);
	}
	return result;
}

void Map::remove_enemies(const std::vector<const Enemy *> enemies)
{
	if (enemies.size() != 0)
		log_printf("Removing %zu enemies", enemies.size());

	std::unordered_map<Section*, std::vector<int>> indexes_to_delete_by_section;
	for (const Enemy* e : enemies) {
		Section* section = this->priv->sections[std::make_pair(get_section_start_coordinate(e->entity.location.x), get_section_start_coordinate(e->entity.location.z))].get();
		indexes_to_delete_by_section[section].push_back(e - &section->enemies[0]);
	}

	for (const auto& pair : indexes_to_delete_by_section) {
		Section* section = pair.first;
		std::vector<int> indexes = pair.second;
		std::sort(indexes.begin(), indexes.end());
		std::reverse(indexes.begin(), indexes.end());

		// pop_back() invalidates iterators and can apparently shrink the vector
		int len = section->enemies.size();
		for (int i : indexes)
			section->enemies[i] = section->enemies[--len];
		section->enemies.erase(section->enemies.begin() + len, section->enemies.end());
	}
}

void Map::move_enemies(vec3 player_location, float dt)
{
	std::vector<Enemy> moved = {};

	for (LocationAndSection las : find_sections_within_circle(*this->priv, player_location.x, player_location.z, 2*VIEW_RADIUS)) {
		std::vector<Enemy>& enemies = las.section->enemies;

		for (int i = enemies.size() - 1; i >= 0; i--) {
			enemies[i].move_towards_player(player_location, *this, dt);

			int startx = get_section_start_coordinate(enemies[i].entity.location.x);
			int startz = get_section_start_coordinate(enemies[i].entity.location.z);
			if (startx != las.startx || startz != las.startz) {
				log_printf("Enemy moves to different section");
				moved.push_back(enemies[i]);
				enemies[i] = enemies[enemies.size()-1];
				enemies.pop_back();
			}
		}
	}

	for (const Enemy& e : moved) {
		int startx = get_section_start_coordinate(e.entity.location.x);
		int startz = get_section_start_coordinate(e.entity.location.z);
		Section* section = find_or_add_section(*this->priv, startx, startz);
		section->enemies.push_back(e);
	}
}
