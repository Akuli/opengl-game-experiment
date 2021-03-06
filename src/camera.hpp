#ifndef CAMERA_HPP
#define CAMERA_HPP

#include "linalg.hpp"

/*
It's often handy to have the camera at (0,0,0) pointing to negative z direction.
Coordinates like that are called "camera coordinates", and the usual coordinates
with camera being wherever it is pointing to wherever it points to are "world
coordinates". Both coordinate systems are right-handed with y axis pointing up.
*/
struct Camera {
	vec3 location;
	mat3 world2cam = {
		1,0,0,
		0,1,0,
		0,0,1,
	};
	mat3 cam2world = {
		1,0,0,
		0,1,0,
		0,0,1,
	};
};

#endif   // CAMERA_HPP
