#include "misc.hpp"
#include <cstdlib>

float uniform_random_float(float min, float max)
{
	// I tried writing this in "modern C++" style but that was more complicated
	return lerp(min, max, std::rand() / (float)RAND_MAX);
}
