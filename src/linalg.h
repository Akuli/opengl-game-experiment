#ifndef LINALG_H
#define LINALG_H

#include <assert.h>
#include <math.h>
#include <stdbool.h>

typedef struct { float x, y; } vec2;
typedef struct { float x, y, z; } vec3;
typedef struct { float x, y, z, w; } vec4;
typedef struct { float rows[2][2]; } mat2;
typedef struct { float rows[3][3]; } mat3;

// should be safe to memcpy() between vectors/matrices and float arrays
static_assert(sizeof(vec2) == 2*sizeof(float), "");
static_assert(sizeof(vec3) == 3*sizeof(float), "");
static_assert(sizeof(vec4) == 4*sizeof(float), "");
static_assert(sizeof(mat2) == 2*2*sizeof(float), "");
static_assert(sizeof(mat3) == 3*3*sizeof(float), "");

inline vec2 vec2_add(vec2 a, vec2 b) { return (vec2){a.x+b.x, a.y+b.y }; }
inline vec3 vec3_add(vec3 a, vec3 b) { return (vec3){a.x+b.x, a.y+b.y, a.z+b.z }; }
inline vec4 vec4_add(vec4 a, vec4 b) { return (vec4){a.x+b.x, a.y+b.y, a.z+b.z, a.w+b.w }; }
inline vec2 vec2_sub(vec2 a, vec2 b) { return (vec2){a.x-b.x, a.y-b.y }; }
inline vec3 vec3_sub(vec3 a, vec3 b) { return (vec3){a.x-b.x, a.y-b.y, a.z-b.z }; }
inline vec4 vec4_sub(vec4 a, vec4 b) { return (vec4){a.x-b.x, a.y-b.y, a.z-b.z, a.w-b.w }; }
inline void vec2_add_inplace(vec2 *a, vec2 b) { a->x += b.x; a->y += b.y; }
inline void vec3_add_inplace(vec3 *a, vec3 b) { a->x += b.x; a->y += b.y; a->z += b.z; }
inline void vec4_add_inplace(vec4 *a, vec4 b) { a->x += b.x; a->y += b.y; a->z += b.z; a->w += b.w; }
inline void vec2_sub_inplace(vec2 *a, vec2 b) { a->x -= b.x; a->y -= b.y; }
inline void vec3_sub_inplace(vec3 *a, vec3 b) { a->x -= b.x; a->y -= b.y; a->z -= b.z; }
inline void vec4_sub_inplace(vec4 *a, vec4 b) { a->x -= b.x; a->y -= b.y; a->z -= b.z; a->w -= b.w; }
inline vec2 vec2_mul_float(vec2 v, float f) { return (vec2){v.x*f, v.y*f}; }
inline vec3 vec3_mul_float(vec3 v, float f) { return (vec3){v.x*f, v.y*f, v.z*f}; }
inline vec4 vec4_mul_float(vec4 v, float f) { return (vec4){v.x*f, v.y*f, v.z*f, v.w*f}; }
inline vec4 vec4_mul_float_first3(vec4 v, float f) { return (vec4){v.x*f, v.y*f, v.z*f, v.w}; }
inline void vec2_mul_float_inplace(vec2 *v, float f) { v->x *= f; v->y *= f; }
inline void vec3_mul_float_inplace(vec3 *v, float f) { v->x *= f; v->y *= f; v->z *= f; }
inline void vec4_mul_float_inplace(vec4 *v, float f) { v->x *= f; v->y *= f; v->z *= f; v->w *= f; }
inline void vec4_mul_float_first3_inplace(vec4 *v, float f) { v->x *= f; v->y *= f; v->z *= f; }
inline float lerp(float a, float b, float t) { return a + (b-a)*t; }
inline vec2 vec2_lerp(vec2 a, vec2 b, float t) { return vec2_add(a, vec2_mul_float(vec2_sub(b,a), t)); }
inline vec3 vec3_lerp(vec3 a, vec3 b, float t) { return vec3_add(a, vec3_mul_float(vec3_sub(b,a), t)); }
inline vec4 vec4_lerp(vec4 a, vec4 b, float t) { return vec4_add(a, vec4_mul_float(vec4_sub(b,a), t)); }
inline float unlerp(float a, float b, float lerped) { return (lerped-a)/(b-a); }
inline vec3 vec3_cross(vec3 a, vec3 b) { return (vec3){ a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x }; }
inline float vec2_dot(vec2 a, vec2 b) { return a.x*b.x + a.y*b.y; }
inline float vec3_dot(vec3 a, vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
inline float vec4_dot(vec4 a, vec4 b) { return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w; }

inline float mat3_det(mat3 M)
{
	return M.rows[0][0]*M.rows[1][1]*M.rows[2][2]
		- M.rows[0][0]*M.rows[1][2]*M.rows[2][1]
		- M.rows[0][1]*M.rows[1][0]*M.rows[2][2]
		+ M.rows[0][1]*M.rows[1][2]*M.rows[2][0]
		+ M.rows[0][2]*M.rows[1][0]*M.rows[2][1]
		- M.rows[0][2]*M.rows[1][1]*M.rows[2][0];
}

inline vec2 mat2_mul_vec2(mat2 M, vec2 v) { return (vec2){
	v.x*M.rows[0][0] + v.y*M.rows[0][1],
	v.x*M.rows[1][0] + v.y*M.rows[1][1],
};}

inline vec3 mat3_mul_vec3(mat3 M, vec3 v) { return (vec3){
	v.x*M.rows[0][0] + v.y*M.rows[0][1] + v.z*M.rows[0][2],
	v.x*M.rows[1][0] + v.y*M.rows[1][1] + v.z*M.rows[1][2],
	v.x*M.rows[2][0] + v.y*M.rows[2][1] + v.z*M.rows[2][2],
};}

mat2 mat2_rotation(float angle);

/*
Slow-ish to compute because uses trig funcs, so don't call this in a loop.

The angle is in radians. If x axis is right, y is up, and z is back, then this
works so that a bigger angle means clockwise if viewed from above. If viewed from
below, then it rotates counter-clockwise instead, so it's just like unit circle
trig in high school, except that you have z axis instead of y axis. For example:
- angle=-pi/2 means that (1,0,0) gets rotated to (0,0,-1)
- angle=0 gives identity matrix
- angle=pi/2 means that (1,0,0) gets rotated to (0,0,1)
- angle=pi means that (1,0,0) gets rotated to (-1,0,0)
*/
mat3 mat3_rotation_xz(float angle);

// any plane in 3D, behaves nicely no matter which way plane is oriented
struct Plane {
	// equation of plane represented as:  (x,y,z) dot normal = constant
	vec3 normal;
	float constant;
};

// Transform each point of the plane, resulting in a new plane
void plane_apply_mat3_INVERSE(struct Plane *pl, mat3 inverse);
void plane_move(struct Plane *pl, vec3 mv);

inline bool plane_whichside(const struct Plane pl, vec3 v)
{
	return vec3_dot(pl.normal, v) > pl.constant;
}

#endif
