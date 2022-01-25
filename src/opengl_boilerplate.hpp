#ifndef OPENGL_BOILERPLATE_HPP
#define OPENGL_BOILERPLATE_HPP

#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <string>

class OpenglBoilerplate {
public:
	SDL_Window *window;

	OpenglBoilerplate();
	~OpenglBoilerplate();
	OpenglBoilerplate(const OpenglBoilerplate&) = delete;

	static GLuint create_shader_program(const std::string& vertex_shader);

private:
	SDL_GLContext ctx;
};

#endif
