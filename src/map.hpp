#ifndef MAP_H
#define MAP_H

#include "camera.hpp"
#include "linalg.hpp"

#include <memory>
#include <vector>
#include <unordered_map>

namespace MapPrivate {
	struct IntPairHasher { size_t operator() (const std::pair<int,int> &pair) const; };
	int get_section_start_coordinate(float val);
	extern const int section_size;

	// not gonna shit all over my h++ file with private structs and methods, sorry
	struct MapData;
}


// Partitions the world into sections to make finding nearby objects easier
template<typename T, typename GetLocation> class SectionedStorage
{
public:
	// TODO: don't return a vector, some kind of iterator instead?
	std::vector<std::reference_wrapper<T>> find_within_circle(float center_x, float center_z, float radius) const {
		using MapPrivate::section_size;
		using MapPrivate::get_section_start_coordinate;

		std::vector<T> result = {};
		int startx_min = get_section_start_coordinate(center_x - radius);
		int startx_max = get_section_start_coordinate(center_x + radius);
		int startz_min = get_section_start_coordinate(center_z - radius);
		int startz_max = get_section_start_coordinate(center_z + radius);

		for (int startx = startx_min; startx <= startx_max; startx += section_size) {
			for (int startz = startz_min; startz <= startz_max; startz += section_size) {
				// TODO: skip if section entirely outside circle
				auto find_result = this->objects.find(std::make_pair(startx, startz));
				if (find_result == this->objects.end())
					continue;

				const std::vector<T>& values = find_result->second;
				for (const T& value : values)
					result.push_back(std::reference_wrapper<T>(value));
			}
		}
		return result;
	}

	void add_object(T&& object) {
		using MapPrivate::get_section_start_coordinate;
		int startx = get_section_start_coordinate(GetLocation{}(object).x);
		int startz = get_section_start_coordinate(GetLocation{}(object).z);
		std::pair<int,int> key = { startx, startz };

		if (this->objects.find(key) == this->objects.end())
			this->objects[key] = std::vector<T>{};
		this->objects[key].push_back(object);
		this->count++;
	}

	int size() const { return this->count; }

private:
	int count;
	std::unordered_map<std::pair<int, int>, std::vector<T>, MapPrivate::IntPairHasher> objects;
};


class Map {
public:
	Map();
	~Map();
	Map(const Map&) = delete;

	// Methods are not const, because map may be expanded dynamically when they are called

	float get_height(float x, float z);
	vec3 get_normal_vector(float x, float z);  // arbitrary length, points away from surface
	void render(const Camera& camera);

private:
	std::unique_ptr<MapPrivate::MapData> priv;
};

#endif
