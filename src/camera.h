#ifndef CAMERA_H
#define CAMERA_H

#include <SDL2/SDL.h>
#include "linalg.h"

// surfaces can be smaller than this, but these are handy for array sizes
#define CAMERA_SCREEN_WIDTH 800
#define CAMERA_SCREEN_HEIGHT 600

/*
It's often handy to have the camera at (0,0,0) pointing to negative z direction.
Coordinates like that are called "camera coordinates", and the usual coordinates
with camera being wherever it is pointing to wherever it points to are "world
coordinates". Both coordinate systems are right-handed with y axis pointing up.
*/
struct Camera {
	Vec3 location;
	Mat3 world2cam, cam2world;  // If you set one, please also set the other
};

/*
The conversion between these consists of a rotation about the camera location and
offsetting by the camera location vector. This is what these functions do, but
there may be good reasons to not use these functions.
*/
inline Vec3 camera_point_world2cam(const struct Camera *cam, Vec3 v)
{
	return mat3_mul_vec3(cam->world2cam, vec3_sub(v, cam->location));
}
inline Vec3 camera_point_cam2world(const struct Camera *cam, Vec3 v)
{
	return vec3_add(mat3_mul_vec3(cam->cam2world, v), cam->location);
}


#endif   // CAMERA_H
