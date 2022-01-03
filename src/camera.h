#ifndef CAMERA_H
#define CAMERA_H

#include <SDL2/SDL.h>
#include "linalg.h"

// surfaces can be smaller than this, but these are handy for array sizes
#define CAMERA_SCREEN_WIDTH 800
#define CAMERA_SCREEN_HEIGHT 600

#define CAMERA_FPS 60

/*
It's often handy to have the camera at (0,0,0) pointing to negative z direction.
Coordinates like that are called "camera coordinates", and the usual coordinates
with camera being wherever it is pointing to wherever it points to are "world
coordinates". Both coordinate systems are right-handed with y axis pointing up.
*/
struct Camera {
	Vec3 location;
	SDL_Surface *surface;
	float screencentery;  // e.g. surface->h/2
	Mat3 world2cam, cam2world;  // If you set one, please also set the other

	/*
	For checking whether an object is visible or not, we split the world
	into visible and invisible parts with planes. The normal vector of each
	visibility plane points to the visible side. So, for a point to be
	visible, plane_whichside() must return true for each visibility plane.
	*/
	struct Plane visplanes[5];
};

/*
visplanes[CAMERA_CAMPLANE_IDX] is the visibility plane that corresponds
to object not being behind camera
*/
#define CAMERA_CAMPLANE_IDX 0

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

/*
When mapping a point given in camera coordinates to a pixel on an SDL surface, we
only use the ratios point.z/point.x and point.z/point.y; we don't need to know
anything else about the point. I call these ratios xzr and yzr, short for "x to z
ratio" and "y to z ratio".

Hints for figuring out the formulas:
- Usually z is negative, so xzr has the opposite sign as x, and yzr has the
  opposite sign of y.
- For x=0 and y=0, we want the center of the SDL surface.
- More x means right, which means means more screen x. More y means up, which
  means *less* screen y. That's how coordinates work in most 2D graphics things.
*/
#define SCALING_FACTOR 300.f
inline float camera_xzr_to_screenx(const struct Camera *cam, float xzr) { return (float)cam->surface->w/2 - SCALING_FACTOR*xzr; }
inline float camera_yzr_to_screeny(const struct Camera *cam, float yzr) { return cam->screencentery + SCALING_FACTOR*yzr; }
inline float camera_screenx_to_xzr(const struct Camera *cam, float screenx) { return (-screenx + (float)cam->surface->w/2)/SCALING_FACTOR; }
inline float camera_screeny_to_yzr(const struct Camera *cam, float screeny) { return (screeny - cam->screencentery)/SCALING_FACTOR; }
#undef SCALING_FACTOR

// call this after changing camera location or cam2world/world2cam matrices
void camera_update_visplanes(struct Camera *cam);

inline Vec2 camera_point_cam2screen(const struct Camera *cam, Vec3 pt)
{
	SDL_assert(pt.z < 0);   // point must be in front of camera
	return (Vec2){
		.x = camera_xzr_to_screenx(cam, pt.x/pt.z),
		.y = camera_yzr_to_screeny(cam, pt.y/pt.z),
	};
}

void camera_drawline(const struct Camera *cam, Vec3 start3, Vec3 end3);


#endif   // CAMERA_H
