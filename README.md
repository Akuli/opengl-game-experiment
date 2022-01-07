Getting started:

	$ sudo apt install git gcc make libsdl2-dev libglew-dev
	$ make -j2
	$ ./game

If your graphics card drivers don't support new enough opengl,
you can use software rendering instead, but this will lead to
100% CPU usage and a low frame rate:

	$ MESA_LOADER_DRIVER_OVERRIDE=llvmpipe ./game
