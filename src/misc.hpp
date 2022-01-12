#ifndef MISC_H
#define MISC_H

template<typename T> T lerp(T a, T b, float t) { return a + (b-a)*t; }
inline float unlerp(float a, float b, float lerped) { return (lerped-a)/(b-a); }

float uniform_random_float(float min, float max);

#endif
