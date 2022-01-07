#include <GL/glew.h>
#include <SDL2/SDL.h>
#include "log.h"
#include "opengl_boilerplate.h"
#include "camera.h"

// use glDeleteShader afterwards
static GLuint create_shader(GLenum type, const char *source, const char *shadername)
{
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);

	GLint status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE) {
		GLint loglen;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &loglen);
		char *log = calloc(1, loglen+1);
		SDL_assert(log);
		glGetShaderInfoLog(shader, loglen, NULL, log);
		log_printf_abort("compiling shader \"%s\" failed: %s", shadername, log);
	}

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
		char *log = calloc(1, loglen+1);
		SDL_assert(log);
		glGetProgramInfoLog(prog, loglen, NULL, log);
		log_printf_abort("linking shader program failed: %s", log);
	}
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

	GLenum tmp;
	if ((tmp = glewInit()) != GLEW_OK) {
		fprintf(stderr, "Error in glewInit(): %s\n", glewGetErrorString(tmp));
		abort();
	}

	// This makes our buffer swap syncronized with the monitor's vertical refresh
	int ret = SDL_GL_SetSwapInterval(1);
	SDL_assert(ret == 0);

	// Run once for each vertex (corner of triangle)
	const char *vertex_shader =
		"#version 330\n"
		"\n"
		"layout(location = 0) in vec3 position;\n"
		"smooth out vec4 vertexToFragmentColor;\n"
		"\n"
		"void main(void)\n"
		"{\n"
		"    // Other components of (x,y,z,w) will be implicitly divided by w\n"
		"    // Resulting z will be used in z-buffer\n"
		"    gl_Position = vec4(position.x, position.y, 1, -position.z);\n"
		"\n"
		"    vertexToFragmentColor.x = 1;\n"
		"    vertexToFragmentColor.y = 0;\n"
		"    vertexToFragmentColor.z = 0;\n"
		"    vertexToFragmentColor.w = 1;\n"
		"}\n"
		;

	// Run once for each drawn pixel
	const char *fragment_shader =
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

	GLuint prog = glCreateProgram();

	GLuint shaders[] = {
		create_shader(GL_VERTEX_SHADER, vertex_shader, "vertex_shader"),
		create_shader(GL_FRAGMENT_SHADER, fragment_shader, "fragment_shader"),
	};
	for (int i = 0; i < sizeof(shaders)/sizeof(shaders[0]); i++)
		glAttachShader(prog, shaders[i]);
	link_program(prog);
	for (int i = 0; i < sizeof(shaders)/sizeof(shaders[0]); i++)
		glDetachShader(prog, shaders[i]);
	for (int i = 0; i < sizeof(shaders)/sizeof(shaders[0]); i++)
		glDeleteShader(shaders[i]);
	glUseProgram(prog);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_GREATER);
	glClearDepth(0);

	// TODO: put these to map.c
	/*
	glGenBuffers(1, &vertex_buffer_object);
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_object);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data), vertex_data, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	*/

	// TODO: understand what these do
	GLuint vertarr;
	glGenVertexArrays(1, &vertarr);
	glBindVertexArray(vertarr);

	glViewport(0, 0, CAMERA_SCREEN_WIDTH, CAMERA_SCREEN_HEIGHT);

	return (struct OpenglBoilerplateState){wnd, ctx};
}

void opengl_boilerplate_quit(const struct OpenglBoilerplateState *state)
{
	SDL_GL_DeleteContext(state->ctx);
	SDL_DestroyWindow(state->window);
	SDL_Quit();
}
