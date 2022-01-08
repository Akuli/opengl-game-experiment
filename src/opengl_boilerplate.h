#ifndef OPENGL_BOILERPLATE_H
#define OPENGL_BOILERPLATE_H

#include <GL/glew.h>
#include <SDL2/SDL.h>

struct OpenglBoilerplateState {
	SDL_Window *window;
	SDL_GLContext *ctx;
};

struct OpenglBoilerplateState opengl_boilerplate_init(void);
GLuint opengl_boilerplate_create_shader_program(const char *vertex_shader, const char *fragment_shader);
void opengl_boilerplate_quit(const struct OpenglBoilerplateState *state);

#endif
