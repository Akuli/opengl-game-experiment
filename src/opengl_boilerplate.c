#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <string.h>
#include "log.h"
#include "opengl_boilerplate.h"
#include "camera.h"

// use glDeleteShader afterwards
static GLuint create_shader(GLenum type, const char *source, const char *shadername)
{
	const char *boilerplate =
		"vec4 darkerAtDistance(in vec3 brightColor, in vec3 locationFromCamera)\n"
		"{\n"
		"    vec3 rgb = brightColor * exp(-0.0003*pow(30+length(locationFromCamera),2));\n"
		"    return vec4(rgb.x, rgb.y, rgb.z, 1);\n"
		"}\n"
		"\n"
		"vec4 locationFromCameraToGlPosition(in vec3 locationFromCamera)\n"
		"{\n"
		"    // Other components of (x,y,z,w) will be implicitly divided by w.\n"
		"    // Resulting z will be used in z-buffer.\n"
		"    return vec4(locationFromCamera.x, locationFromCamera.y, 1, -locationFromCamera.z);\n"
		"}\n"
		;

	const char *boilerplateloc = strstr(source, "BOILERPLATE_GOES_HERE");
	char *tmp;
	if (boilerplateloc) {
		tmp = calloc(1, strlen(source) + strlen(boilerplate) + 1);
		if (!tmp)
			log_printf_abort("not enough memory");

		strncpy(tmp, source, boilerplateloc-source);
		strcat(tmp, boilerplate);
		strcat(tmp, boilerplateloc + strlen("BOILERPLATE_GOES_HERE"));
		source = tmp;
	} else {
		tmp = NULL;
	}

	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);

	GLint status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE) {
		GLint loglen;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &loglen);
		char log[1000] = {0};
		glGetShaderInfoLog(shader, sizeof log - 1, NULL, log);
		log_printf_abort("compiling shader \"%s\" failed: %s", shadername, log);
	}

	free(tmp);
	return shader;
}

static void link_program(GLuint prog)
{
	glLinkProgram(prog);

	GLint status;
	glGetProgramiv(prog, GL_LINK_STATUS, &status);
	if (status == GL_FALSE) {
		GLint loglen;
		glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &loglen);
		char log[1000] = {0};
		glGetProgramInfoLog(prog, sizeof log - 1, NULL, log);
		log_printf_abort("linking shader program failed: %s", log);
	}
}

GLuint opengl_boilerplate_create_shader_program(const char *vertex_shader, const char *fragment_shader)
{
	SDL_assert(vertex_shader);
	if (!fragment_shader) {
		fragment_shader =
			"#version 330\n"
			"\n"
			"smooth in vec4 vertexToFragmentColor;\n"
			"out vec4 outColor;\n"
			"\n"
			"void main(void)\n"
			"{\n"
			"    outColor = vertexToFragmentColor;\n"
			"}\n"
			;
	}

	GLuint prog = glCreateProgram();

	GLuint vs = create_shader(GL_VERTEX_SHADER, vertex_shader, "vertex_shader");
	GLuint fs = create_shader(GL_FRAGMENT_SHADER, fragment_shader, "fragment_shader");
	glAttachShader(prog, vs);
	glAttachShader(prog, fs);
	link_program(prog);
	glDetachShader(prog, vs);
	glDetachShader(prog, fs);
	glDeleteShader(vs);
	glDeleteShader(fs);

	return prog;
}

struct OpenglBoilerplateState opengl_boilerplate_init(void)
{
	SDL_assert(SDL_Init(SDL_INIT_VIDEO) == 0);

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	SDL_Window *wnd = SDL_CreateWindow(
		"title", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		CAMERA_SCREEN_WIDTH, CAMERA_SCREEN_HEIGHT,
		SDL_WINDOW_OPENGL);
	if (!wnd)
		log_printf_abort("SDL_CreateWindow failed: %s", SDL_GetError());

	SDL_GLContext *ctx = SDL_GL_CreateContext(wnd);
	if (!ctx)
		log_printf_abort("SDL_GL_CreateContext failed: %s", SDL_GetError());

	// TODO: understand why exactly i need sdl2+glew+opengl
	GLenum tmp;
	if ((tmp = glewInit()) != GLEW_OK)
		log_printf_abort("Error in glewInit(): %s", glewGetErrorString(tmp));

	// This makes our buffer swap syncronized with the monitor's vertical refresh.
	// Fails when using software rendering (see README)
	int ret = SDL_GL_SetSwapInterval(1);
	if (ret != 0)
		log_printf("SDL_GL_SetSwapInterval failed: %s", SDL_GetError());

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_GREATER);
	glClearDepth(0);

	// TODO: understand what these do
	GLuint vertarr;
	glGenVertexArrays(1, &vertarr);
	glBindVertexArray(vertarr);

	glViewport(0, 0, CAMERA_SCREEN_WIDTH, CAMERA_SCREEN_HEIGHT);

	return (struct OpenglBoilerplateState){ .window = wnd, .ctx = ctx };
}

void opengl_boilerplate_quit(const struct OpenglBoilerplateState *state)
{
	SDL_GL_DeleteContext(state->ctx);
	SDL_DestroyWindow(state->window);
	SDL_Quit();
}
