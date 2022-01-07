#include "camera.h"
#include "linalg.h"

// non-static inlines are weird in c
extern inline Vec3 camera_point_world2cam(const struct Camera *cam, Vec3 v);
extern inline Vec3 camera_point_cam2world(const struct Camera *cam, Vec3 v);
