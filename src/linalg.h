#ifndef LINALG_H
#define LINALG_H

#include <stdbool.h>

typedef struct { float x, y; } Vec2;
typedef struct { float x, y, z; } Vec3;
typedef struct { int x, y, z; } iVec3;
typedef struct { float rows[3][3]; } Mat3;

inline Vec3 vec3_add(Vec3 a, Vec3 b) { return (Vec3){a.x+b.x, a.y+b.y, a.z+b.z }; }
inline Vec3 vec3_sub(Vec3 a, Vec3 b) { return (Vec3){a.x-b.x, a.y-b.y, a.z-b.z }; }
inline float vec3_dot(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }

inline float mat3_det(Mat3 M)
{
	return M.rows[0][0]*M.rows[1][1]*M.rows[2][2]
		- M.rows[0][0]*M.rows[1][2]*M.rows[2][1]
		- M.rows[0][1]*M.rows[1][0]*M.rows[2][2]
		+ M.rows[0][1]*M.rows[1][2]*M.rows[2][0]
		+ M.rows[0][2]*M.rows[1][0]*M.rows[2][1]
		- M.rows[0][2]*M.rows[1][1]*M.rows[2][0];
}

inline Vec3 mat3_mul_vec3(Mat3 M, Vec3 v) { return (Vec3){
	v.x*M.rows[0][0] + v.y*M.rows[0][1] + v.z*M.rows[0][2],
	v.x*M.rows[1][0] + v.y*M.rows[1][1] + v.z*M.rows[1][2],
	v.x*M.rows[2][0] + v.y*M.rows[2][1] + v.z*M.rows[2][2],
};}

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
Mat3 mat3_rotation_xz(float angle);

// any plane in 3D, behaves nicely no matter which way plane is oriented
struct Plane {
	// equation of plane represented as:  (x,y,z) dot normal = constant
	Vec3 normal;
	float constant;
};

// Transform each point of the plane, resulting in a new plane
void plane_apply_mat3_INVERSE(struct Plane *pl, Mat3 inverse);
void plane_move(struct Plane *pl, Vec3 mv);

inline bool plane_whichside(const struct Plane pl, Vec3 v)
{
	return vec3_dot(pl.normal, v) > pl.constant;
}

#endif
