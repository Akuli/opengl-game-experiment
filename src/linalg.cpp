#include <math.h>
#include "linalg.hpp"

extern inline vec2 vec2_add(vec2 a, vec2 b);
extern inline vec3 vec3_add(vec3 a, vec3 b);
extern inline vec4 vec4_add(vec4 a, vec4 b);
extern inline vec2 vec2_sub(vec2 a, vec2 b);
extern inline vec3 vec3_sub(vec3 a, vec3 b);
extern inline vec4 vec4_sub(vec4 a, vec4 b);
extern inline void vec2_add_inplace(vec2 *a, vec2 b);
extern inline void vec3_add_inplace(vec3 *a, vec3 b);
extern inline void vec4_add_inplace(vec4 *a, vec4 b);
extern inline void vec2_sub_inplace(vec2 *a, vec2 b);
extern inline void vec3_sub_inplace(vec3 *a, vec3 b);
extern inline void vec4_sub_inplace(vec4 *a, vec4 b);
extern inline vec2 vec2_mul_float(vec2 v, float f);
extern inline vec3 vec3_mul_float(vec3 v, float f);
extern inline vec4 vec4_mul_float(vec4 v, float f);
extern inline vec4 vec4_mul_float_first3(vec4 v, float f);
extern inline void vec2_mul_float_inplace(vec2 *v, float f);
extern inline void vec3_mul_float_inplace(vec3 *v, float f);
extern inline void vec4_mul_float_inplace(vec4 *v, float f);
extern inline void vec4_mul_float_first3_inplace(vec4 *v, float f);
extern inline float lerp(float a, float b, float t);
extern inline vec2 vec2_lerp(vec2 a, vec2 b, float t);
extern inline vec3 vec3_lerp(vec3 a, vec3 b, float t);
extern inline vec4 vec4_lerp(vec4 a, vec4 b, float t);
extern inline float unlerp(float a, float b, float lerped);
extern inline vec3 vec3_cross(vec3 a, vec3 b);
extern inline float vec2_dot(vec2 a, vec2 b);
extern inline float vec3_dot(vec3 a, vec3 b);
extern inline float vec4_dot(vec4 a, vec4 b);
extern inline float mat3_det(mat3 M);
extern inline vec2 mat2_mul_vec2(mat2 M, vec2 v);
extern inline vec3 mat3_mul_vec3(mat3 M, vec3 v);
extern inline bool plane_whichside(const struct Plane pl, vec3 v);

mat2 mat2_rotation(float angle)
{
	float cos = cosf(angle);
	float sin = sinf(angle);
	return mat2{
		cos, -sin,
		sin, cos,
	};
}

mat3 mat3_rotation_xz(float angle)
{
	float cos = cosf(angle);
	float sin = sinf(angle);
	return mat3{
		cos, 0, -sin,
		0,   1, 0,
		sin, 0, cos,
	};
}

static void swap(float *a, float *b)
{
	float tmp = *a;
	*a = *b;
	*b = tmp;
}

static void transpose(mat3 *M)
{
	swap(&M->rows[1][0], &M->rows[0][1]);
	swap(&M->rows[2][0], &M->rows[0][2]);
	swap(&M->rows[2][1], &M->rows[1][2]);
}

void plane_apply_mat3_INVERSE(struct Plane *pl, mat3 inverse)
{
	/*
	The plane equation can be written as ax+by+cz = constant. By thinking of
	numbers as 1x1 matrices, we can write that as

		         _   _
		        |  x  |
		[a b c] |  y  | = constant.
		        |_ z _|

	Here we have

		            _   _
		       T   |  a  |
		[a b c]  = |  b  | = pl->normal.
		           |_ c _|

	How to apply a matrix to the plane? Consider the two planes that we should
	have before and after applying the matrix. A point is on the plane after
	applying the transform if and only if the INVERSE transformed point is on the
	plane before applying the transform. This means that the plane we have after
	the transform has the equation

		              _   _
		             |  x  |
		[a b c] M^-1 |  y  | = constant,
		             |_ z _|

	and from linear algebra, we know that

		                           _   _    T
		               /       T  |  a  | \
		[a b c] M^-1 = | (M^-1)   |  b  | |
		               \          |_ c _| /
	*/
	transpose(&inverse);
	pl->normal = mat3_mul_vec3(inverse, pl->normal);
}

void plane_move(struct Plane *pl, vec3 mv)
{
	pl->constant += vec3_dot(pl->normal, mv);
}
