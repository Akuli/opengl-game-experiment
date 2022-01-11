#include "linalg.hpp"
#include <cmath>

float mat3::det() const
{
	return this->rows[0][0]*this->rows[1][1]*this->rows[2][2]
		- this->rows[0][0]*this->rows[1][2]*this->rows[2][1]
		- this->rows[0][1]*this->rows[1][0]*this->rows[2][2]
		+ this->rows[0][1]*this->rows[1][2]*this->rows[2][0]
		+ this->rows[0][2]*this->rows[1][0]*this->rows[2][1]
		- this->rows[0][2]*this->rows[1][1]*this->rows[2][0];
}

mat2 mat2::rotation(float angle)
{
	using std::sin, std::cos;
	return mat2{
		cos(angle), -sin(angle),
		sin(angle), cos(angle),
	};
}

mat3 mat3::rotation_about_y(float angle)
{
	using std::sin, std::cos;
	return mat3{
		cos(angle), 0, -sin(angle),
		0,          1, 0,
		sin(angle), 0, cos(angle),
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

void Plane::apply_matrix_INVERSE(mat3 inverse)
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
	this->normal = inverse * this->normal;
}

void Plane::move(vec3 mv)
{
	this->constant += this->normal.dot(mv);
}
