#ifndef OPENGL_BOILERPLATE_H
#define OPENGL_BOILERPLATE_H

#include <SDL2/SDL.h>

struct OpenglBoilerplateState {
	SDL_Window *window;
	SDL_GLContext *ctx;
};

struct OpenglBoilerplateState opengl_boilerplate_init(void);
void opengl_boilerplate_quit(const struct OpenglBoilerplateState *state);

#endif
