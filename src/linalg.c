#include <math.h>
#include "linalg.h"

extern inline Vec3 vec3_add(Vec3 a, Vec3 b);
extern inline void vec3_add_inplace(Vec3 *a, Vec3 b);
extern inline Vec3 vec3_sub(Vec3 a, Vec3 b);
extern inline float vec3_dot(Vec3 a, Vec3 b);
extern inline float mat3_det(Mat3 M);
extern inline Vec3 mat3_mul_vec3(Mat3 M, Vec3 v);
extern inline bool plane_whichside(const struct Plane pl, Vec3 v);

Mat3 mat3_rotation_xz(float angle)
{
	float cos = cosf(angle);
	float sin = sinf(angle);
	return (Mat3){ .rows = {
		{ cos, 0, -sin },
		{ 0,   1, 0    },
		{ sin, 0, cos  },
	}};
}

static void swap(float *a, float *b)
{
	float tmp = *a;
	*a = *b;
	*b = tmp;
}

static void transpose(Mat3 *M)
{
	swap(&M->rows[1][0], &M->rows[0][1]);
	swap(&M->rows[2][0], &M->rows[0][2]);
	swap(&M->rows[2][1], &M->rows[1][2]);
}

void plane_apply_mat3_INVERSE(struct Plane *pl, Mat3 inverse)
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

void plane_move(struct Plane *pl, Vec3 mv)
{
	pl->constant += vec3_dot(pl->normal, mv);
}
