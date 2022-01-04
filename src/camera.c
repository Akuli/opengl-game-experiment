#include "camera.h"
#include <assert.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "linalg.h"

#define min(a,b) ((a)<(b) ? (a) : (b))
#define max(a,b) ((a)>(b) ? (a) : (b))

static inline void clamp(int *val, int lo, int hi) {
	if (*val < lo) *val = lo;
	if (*val > hi) *val = hi;
}

// non-static inlines are weird in c
extern inline float camera_xzr_to_screenx(const struct Camera *cam, float xzr);
extern inline float camera_yzr_to_screeny(const struct Camera *cam, float yzr);
extern inline float camera_screenx_to_xzr(const struct Camera *cam, float screenx);
extern inline float camera_screeny_to_yzr(const struct Camera *cam, float screeny);
extern inline Vec2 camera_point_cam2screen(const struct Camera *cam, Vec3 pt);
extern inline Vec3 camera_point_world2cam(const struct Camera *cam, Vec3 v);
extern inline Vec3 camera_point_cam2world(const struct Camera *cam, Vec3 v);

void camera_update_visplanes(struct Camera *cam)
{
	// see also CAMERA_CAMPLANE_IDX
	struct Plane pl[] = {
		// z=0, with normal vector to negative side (that's where camera is looking)
		{ .normal = {0, 0, -1}, .constant = 0 },

		// left side of view: x/z = xzr, aka 1x + 0y + (-xzr)z = 0, normal vector to positive x direction
		{ .normal = {1, 0, -camera_screenx_to_xzr(cam, 0)}, .constant = 0 },

		// right side of view, normal vector to negative x direction
		{ .normal = {-1, 0, camera_screenx_to_xzr(cam, (float)cam->surface->w)}, .constant = 0 },

		// top, normal vector to negative y direction
		{ .normal = {0, -1, camera_screeny_to_yzr(cam, 0)}, .constant = 0 },

		// bottom, normal vector to positive y direction
		{ .normal = {0, 1, -camera_screeny_to_yzr(cam, (float)cam->surface->h)}, .constant = 0 },
	};

	// Convert from camera coordinates to world coordinates
	for (int i = 0; i < sizeof(pl)/sizeof(pl[0]); i++) {
		plane_apply_mat3_INVERSE(&pl[i], cam->world2cam);
		plane_move(&pl[i], cam->location);
	}

	static_assert(sizeof(pl) == sizeof(cam->visplanes), "");
	memcpy(cam->visplanes, pl, sizeof(pl));
}

static void draw_2d_rect(SDL_Surface *surf, SDL_Rect r)
{
	if (r.w < 0) {
		r.w = abs(r.w);
		r.x -= r.w;
	}
	if (r.h < 0) {
		r.h = abs(r.h);
		r.y -= r.h;
	}

	SDL_Rect clip;
	if (SDL_IntersectRect(&r, &(SDL_Rect){0,0,surf->w,surf->h}, &clip)) {
		uint32_t color = SDL_MapRGB(surf->format, 0xff, 0x00, 0x00);
		SDL_FillRect(surf, &clip, color);
	}
}

static bool point_is_in_view(const struct Camera *cam, Vec3 p)
{
	for (int i = 0; i < sizeof(cam->visplanes)/sizeof(cam->visplanes[0]); i++) {
		if (!plane_whichside(cam->visplanes[i], p))
			return false;
	}
	return true;
}

void camera_drawline(const struct Camera *cam, Vec3 start3, Vec3 end3)
{
	// make sure that camera_point_cam2screen will work
	if (!plane_whichside(cam->visplanes[CAMERA_CAMPLANE_IDX], start3) ||
		!plane_whichside(cam->visplanes[CAMERA_CAMPLANE_IDX], end3))
	{
		return;
	}

	if (!point_is_in_view(cam, start3) && !point_is_in_view(cam, end3))
		return;

	Vec2 start = camera_point_cam2screen(cam, camera_point_world2cam(cam, start3));
	Vec2 end = camera_point_cam2screen(cam, camera_point_world2cam(cam, end3));
	int x1 = (int)start.x;
	int y1 = (int)start.y;
	int x2 = (int)end.x;
	int y2 = (int)end.y;

	if (x1 == x2) {
		// Vertical line
		draw_2d_rect(cam->surface, (SDL_Rect){ x1-1, y1, 3, y2-y1 });
	} else if (y1 == y2) {
		// Horizontal line
		draw_2d_rect(cam->surface, (SDL_Rect){ x1, y1-1, x2-x1, 3 });
	} else if (abs(y2-y1) > abs(x2-x1)) {
		// Many vertical lines
		// Avoid doing many loops outside the screen, that's slow
		int a = max(min(x1, x2), 0), b = min(max(x1, x2), cam->surface->w);
		for (int x = a; x <= b; x++) {
			int y     = y1 + (y2 - y1)*(x   - x1)/(x2 - x1);
			int ynext = y1 + (y2 - y1)*(x+1 - x1)/(x2 - x1);
			clamp(&ynext, min(y1,y2), max(y1,y2));
			draw_2d_rect(cam->surface, (SDL_Rect){ x-1, y, 3, ynext-y });
		}
	} else {
		// Many horizontal lines
		int a = max(min(y1, y2), 0), b = min(max(y1, y2), cam->surface->h);
		for (int y = a; y <= b; y++) {
			int x     = x1 + (x2 - x1)*(y   - y1)/(y2 - y1);
			int xnext = x1 + (x2 - x1)*(y+1 - y1)/(y2 - y1);
			clamp(&xnext, min(x1,x2), max(x1,x2));
			draw_2d_rect(cam->surface, (SDL_Rect){ x, y-1, xnext-x, 3 });
		}
	}
}
