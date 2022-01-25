#ifndef CONFIG_HPP
#define CONFIG_HPP

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

#define CAMERA_HEIGHT 15    // Relative to player's height
#define CAMERA_MIN_HEIGHT 3  // Won't dip any lower than this amount above map surface
#define CAMERA_HORIZONTAL_DISTANCE 20
#define VIEW_RADIUS 80

#define MIN_PHYSICS_STEP_SECONDS 0.02
#define GRAVITY 20
#define PLAYER_MOVING_FORCE 40.0f
#define ENEMY_MOVING_FORCE 30.0f
#define ENEMY_MAX_SPEED 6
#define PLAYER_TURNING_SPEED 1.8f  // radians per second

#define ENEMY_DELAY 1

#endif
