#ifndef CAMERA_H
#define CAMERA_H

#include "linalg.hpp"

/*
It's often handy to have the camera at (0,0,0) pointing to negative z direction.
Coordinates like that are called "camera coordinates", and the usual coordinates
with camera being wherever it is pointing to wherever it points to are "world
coordinates". Both coordinate systems are right-handed with y axis pointing up.
*/
struct Camera {
	vec3 location;
	mat3 world2cam, cam2world;  // If you set one, please also set the other
};

#endif   // CAMERA_H
