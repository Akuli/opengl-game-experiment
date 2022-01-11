#include <GL/glew.h>
#include <SDL2/SDL.h>
#include "log.hpp"
#include "opengl_boilerplate.hpp"
#include "camera.hpp"

// use glDeleteShader afterwards
static GLuint create_shader(GLenum type, const std::string& source, const char *shadername)
{
	std::string marker = "BOILERPLATE_GOES_HERE";
	std::string boilerplate =
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

	std::string::size_type boilerplateloc = source.find(marker);
	std::string actual_source;
	if (boilerplateloc == std::string::npos) {
		actual_source = source;
	} else {
		actual_source = source.substr(0, boilerplateloc) + boilerplate + source.substr(boilerplateloc + marker.length());
	}

	GLuint shader = glCreateShader(type);
	const char *tmp = actual_source.c_str();
	glShaderSource(shader, 1, &tmp, nullptr);
	glCompileShader(shader);

	GLint status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE) {
		GLint loglen;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &loglen);
		char log[1000] = {0};
		glGetShaderInfoLog(shader, sizeof log - 1, nullptr, log);
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
		char log[1000] = {0};
		glGetProgramInfoLog(prog, sizeof log - 1, nullptr, log);
		log_printf_abort("linking shader program failed: %s", log);
	}
}

GLuint OpenglBoilerplate::create_shader_program(const std::string& vertex_shader)
{
	std::string fragment_shader =
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

OpenglBoilerplate::OpenglBoilerplate()
{
	int ret = SDL_Init(SDL_INIT_VIDEO);
	SDL_assert(ret == 0);

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	this->window = SDL_CreateWindow(
		"title", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		CAMERA_SCREEN_WIDTH, CAMERA_SCREEN_HEIGHT,
		SDL_WINDOW_OPENGL);
	if (!this->window)
		log_printf_abort("SDL_CreateWindow failed: %s", SDL_GetError());

	this->ctx = SDL_GL_CreateContext(this->window);
	if (!this->ctx)
		log_printf_abort("SDL_GL_CreateContext failed: %s", SDL_GetError());

	// TODO: understand why exactly i need sdl2+glew+opengl
	GLenum tmp;
	if ((tmp = glewInit()) != GLEW_OK)
		log_printf_abort("Error in glewInit(): %s", glewGetErrorString(tmp));

	// This makes our buffer swap syncronized with the monitor's vertical refresh.
	// Fails when using software rendering (see README)
	ret = SDL_GL_SetSwapInterval(1);
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
}

OpenglBoilerplate::~OpenglBoilerplate()
{
	SDL_GL_DeleteContext(this->ctx);
	SDL_DestroyWindow(this->window);
	SDL_Quit();
}
